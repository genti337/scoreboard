#!/usr/bin/env python3
"""
Generate matrix-ready ESPN team BMPs in images/<league>/ABBREV.bmp.

Examples:
  All leagues:  python3 scripts/get_team_logos2.py
  Fill gaps:    python3 scripts/get_team_logos2.py --missing-only
  One league:   python3 scripts/get_team_logos2.py --league mens-college-basketball
  One team:     python3 scripts/get_team_logos2.py --league college-football --teams OSU
"""

import argparse
import io
import re
from collections import defaultdict
from pathlib import Path
from tempfile import NamedTemporaryFile

import requests
from PIL import Image, ImageChops

TEAMS_URL_TMPL = "https://site.api.espn.com/apis/site/v2/sports/{sport}/{league}/teams?limit=1000"
IMAGE_ROOT = Path(__file__).resolve().parents[1] / "images"
# League slugs are also the directories used by the Pi renderer and controller.
LEAGUE_SPORTS = {
    "nfl": "football",
    "college-football": "football",
    "nba": "basketball",
    "mens-college-basketball": "basketball",
    "mlb": "baseball",
    "college-baseball": "baseball",
}

def fetch_teams_index(sport: str, league: str):
    url = TEAMS_URL_TMPL.format(sport=sport, league=league)
    r = requests.get(url, timeout=20)
    r.raise_for_status()
    return r.json()

def iter_team_objs(teams_index_json):
    # ESPN structure: data -> sports[0] -> leagues[0] -> teams[] -> {team:{...}}
    for sport in (teams_index_json.get("sports") or []):
        for league in (sport.get("leagues") or []):
            for twrap in (league.get("teams") or []):
                team = twrap.get("team") or twrap
                if team:
                    yield team

def pick_smallest_logo_url(team_obj, dark_background=False):
    """Prefer dark-background variants when requested, then compare dimensions."""
    logos = [logo for logo in (team_obj.get("logos") or []) if logo.get("href")]
    if dark_background:
        dark_labels = {"dark", "primary_logo_on_black_color"}
        dark_logos = [logo for logo in logos if dark_labels.intersection(logo.get("rel") or [])]
        logos = dark_logos or logos
    if not logos:
        return None
    logos_sorted = sorted(
        logos,
        key=lambda L: (int(L.get("width", 10**9)), int(L.get("height", 10**9)))
    )
    return logos_sorted[0].get("href")

def fetch_image(url) -> Image.Image:
    r = requests.get(url, timeout=20)
    r.raise_for_status()
    return Image.open(io.BytesIO(r.content)).convert("RGBA")

def crop_transparent_and_near_black(img: Image.Image, near_black_tol=8) -> Image.Image:
    # 1) Trim fully transparent borders
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    alpha = img.split()[-1]
    if alpha.getextrema()[0] < 255:
        bbox = alpha.getbbox()
        return img.crop(bbox) if bbox else img
    # 2) Trim near-black padding
    rgb = img.convert("RGB")
    black = Image.new("RGB", rgb.size, (0, 0, 0))
    diff = ImageChops.difference(rgb, black)
    def thresh(p): return 0 if p <= near_black_tol else 255
    r, g, b = diff.split()
    r = r.point(thresh); g = g.point(thresh); b = b.point(thresh)
    non_black = ImageChops.lighter(ImageChops.lighter(r, g), b)
    bbox = non_black.getbbox()
    if bbox:
        img = img.crop(bbox)
    return img

def resize_to_height(img: Image.Image, target_h=32) -> Image.Image:
    w, h = img.size
    if h <= 0:
        return img
    new_w = max(1, round(w * (target_h / h)))
    return img.resize((new_w, target_h), Image.Resampling.LANCZOS)

def cap_final_width(img: Image.Image, max_w=48) -> Image.Image:
    w, h = img.size
    if w <= max_w:
        return img
    scale = max_w / float(w)
    new_h = max(1, round(h * scale))
    return img.resize((max_w, new_h), Image.Resampling.LANCZOS)

def pad_width_for_stride(img: Image.Image, bpp=24, center=True, bg=(0,0,0,0)):
    """
    Ensure (width * bytes_per_pixel) % 4 == 0 to avoid row padding confusion.
    24 bpp -> width % 4 == 0 ; 32 bpp -> already safe, but we still normalize.
    """
    bytes_per_px = 3 if bpp == 24 else 4
    w, h = img.size
    remainder = (w * bytes_per_px) % 4
    if remainder == 0:
        return img
    add_px = (4 - (w % 4)) % 4 if bpp == 24 else (4 - remainder) // bytes_per_px
    new_w = w + add_px
    canvas = Image.new(img.mode, (new_w, h), bg)
    x = (new_w - w) // 2 if center else 0
    canvas.paste(img, (x, 0))
    return canvas

def remove_alpha_to_rgb(img: Image.Image, bg_color=(0,0,0)) -> Image.Image:
    if img.mode == "RGBA":
        bg = Image.new("RGB", img.size, bg_color)
        bg.paste(img, mask=img.split()[3])
        return bg
    return img.convert("RGB")

def generate_league(sport, league, output_dir, args, requested):
    try:
        teams_index = fetch_teams_index(sport, league)
        output_dir.mkdir(parents=True, exist_ok=True)
    except (requests.RequestException, OSError, ValueError) as error:
        print(f"✗ {league}: {error}")
        return False, set()

    found = set()
    teams_by_abbr = defaultdict(list)
    failed = False
    for team in iter_team_objs(teams_index):
        abbr = (team.get("abbreviation") or "").upper()
        if not re.fullmatch(r"[A-Z0-9][A-Z0-9_-]*", abbr):
            continue
        if requested and abbr not in requested:
            continue
        found.add(abbr)
        if any(logo.get("href") for logo in (team.get("logos") or [])):
            teams_by_abbr[abbr].append(team)

    for abbr, teams in teams_by_abbr.items():
        # Keep the lowest numeric ESPN ID at the legacy path. Other schools
        # sharing its abbreviation get an ID suffix so they cannot overwrite it.
        teams.sort(key=lambda team: int(team["id"]) if str(team.get("id", "")).isdigit() else float("inf"))
        for index, team in enumerate(teams):
            team_id = str(team.get("id", ""))
            if index and not re.fullmatch(r"[0-9]+", team_id):
                print(f"✗ {league}/{abbr}: shared abbreviation without a numeric ESPN team ID")
                failed = True
                continue
            filename = f"{abbr}-{team_id}.bmp" if index else f"{abbr}.bmp"
            out_path = output_dir / filename
            if args.missing_only and out_path.is_file():
                continue
            if not generate_logo(team, out_path, args):
                failed = True

    if not found and not requested:
        print(f"✗ {league}: ESPN returned no teams")
        failed = True
    without_artwork = sorted(abbr for abbr in found - teams_by_abbr.keys() if not (output_dir / f"{abbr}.bmp").is_file())
    if without_artwork:
        print(f"- {league}: ESPN did not provide artwork for {len(without_artwork)} abbreviations"
              + (f" ({', '.join(without_artwork)})" if requested else ""))
        failed = failed or bool(requested)
    return not failed, found


def generate_logo(team, out_path, args):
    try:
        logo_url = pick_smallest_logo_url(team, dark_background=args.dark_background)
        try:
            img = fetch_image(logo_url)
        except (requests.RequestException, OSError):
            default_url = pick_smallest_logo_url(team)
            if not default_url or default_url == logo_url:
                raise
            img = fetch_image(default_url)
        img = crop_transparent_and_near_black(img, near_black_tol=args.black_tol)
        img = resize_to_height(img, target_h=args.height)

        if args.force_square:
            img = cap_final_width(img, max_w=args.height)
            w, h = img.size
            canvas = Image.new("RGBA", (args.height, args.height), (0, 0, 0, 0))
            canvas.paste(img, ((args.height - w) // 2, (args.height - h) // 2))
            img = canvas

        img = cap_final_width(img, max_w=args.max_width)
        img = pad_width_for_stride(img, bpp=args.bpp, center=not args.left_align)
        out_img = remove_alpha_to_rgb(img) if args.bpp == 24 else img.convert("RGBA")

        temp_path = None
        try:
            with NamedTemporaryFile(dir=out_path.parent, suffix=".tmp", delete=False) as temporary:
                temp_path = Path(temporary.name)
                out_img.save(temporary, format="BMP")
            mode = out_path.stat().st_mode & 0o777 if out_path.exists() else 0o644
            temp_path.chmod(mode)
            temp_path.replace(out_path)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)
        print(f"✓ {out_path} ({out_img.width}x{out_img.height}, {args.bpp}bpp)")
        return True
    except (requests.RequestException, OSError, ValueError, TypeError) as error:
        print(f"✗ {out_path}: {error}")
        return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sport", help="ESPN sport. Inferred for supported leagues.")
    ap.add_argument("--league", help="One ESPN league slug. Defaults to all scoreboard leagues.")
    ap.add_argument("--teams", default="", help="Comma-separated team abbreviations. Defaults to all.")
    ap.add_argument("--out", type=Path, help="Output directory for one league, or root for all leagues.")
    ap.add_argument("--missing-only", action="store_true", help="Keep BMPs already on disk.")
    ap.add_argument("--height", type=int, default=32, help="Target pixel height")
    ap.add_argument("--max-width", type=int, default=48, help="Cap final width (px)")
    ap.add_argument("--black-tol", type=int, default=8, help="Near-black trim tolerance")
    artwork = ap.add_mutually_exclusive_group()
    artwork.add_argument("--dark-background", dest="dark_background", action="store_true",
                         help="Prefer ESPN artwork designed for black backgrounds (default).")
    artwork.add_argument("--default-artwork", dest="dark_background", action="store_false",
                         help="Select artwork by size without preferring dark-background variants.")
    ap.set_defaults(dark_background=True)
    ap.add_argument("--bpp", type=int, default=24, choices=[24, 32], help="BMP bits per pixel")
    ap.add_argument("--force-square", action="store_true",
                    help="Pad to height×height after resize (before width cap).")
    ap.add_argument("--left-align", action="store_true",
                    help="Left-align when stride-padding (default center).")
    args = ap.parse_args()
    if args.height < 1 or args.max_width < 1:
        ap.error("--height and --max-width must be positive")
    if args.sport and not args.league:
        # Preserve the old --sport football shortcut for NFL.
        if args.sport != "football":
            ap.error("--sport requires --league")
        args.league = "nfl"
    leagues = LEAGUE_SPORTS
    if args.league:
        sport = args.sport or LEAGUE_SPORTS.get(args.league)
        if not sport:
            ap.error("specify --sport for an ESPN league outside the scoreboard's supported leagues")
        if not all(re.fullmatch(r"[a-z0-9][a-z0-9.-]*", part) for part in (sport, args.league)):
            ap.error("invalid ESPN sport or league slug")
        leagues = {args.league: sport}

    requested = {team.strip().upper() for team in args.teams.split(",") if team.strip()}
    found = set()
    success = True
    for league, sport in leagues.items():
        output_dir = args.out if args.out is not None and args.league else (args.out or IMAGE_ROOT) / league
        ok, matched = generate_league(sport, league, output_dir, args, requested)
        success = success and ok
        found.update(matched)
    if requested - found:
        print(f"✗ Teams not found: {', '.join(sorted(requested - found))}")
        success = False
    return 0 if success else 1

if __name__ == "__main__":
    raise SystemExit(main())
