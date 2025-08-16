#!/usr/bin/env python3
"""
General ESPN logo fetcher -> 32px-tall, stride-safe BMPs saved as ABBREV.bmp.

- Works for NFL, MLB, NCAAF, etc. via --sport and --league.
- Uses ESPN teams API to list teams, chooses the *smallest-width* logo from team['logos'].
- Crops transparent & near-black borders.
- Resizes to height=32 px, preserves aspect ratio.
- Caps final width to 48 px if needed (proportionally).
- Pads width so BMP row stride is naturally aligned (naive BMP readers safe).
- Saves as TEAM_ABBREV.bmp (e.g., BAL.bmp, NYY.bmp, NEB.bmp).

Examples:
  NFL:  python logos_any_league_to_bmp.py --sport football --league nfl
  MLB:  python logos_any_league_to_bmp.py --sport baseball --league mlb
  NCAAF:python logos_any_league_to_bmp.py --sport football --league college-football
  Subset: --teams BAL,KC,NEB,NYY
"""

import os, io, argparse, requests
from PIL import Image, ImageChops

TEAMS_URL_TMPL = "https://site.api.espn.com/apis/site/v2/sports/{sport}/{league}/teams?limit=500"

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

def pick_smallest_logo_url(team_obj):
    """Pick logo with smallest width from team['logos']."""
    logos = team_obj.get("logos") or []
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
    bbox = alpha.getbbox()
    if bbox:
        img = img.crop(bbox)
        alpha = img.split()[-1]
    # 2) Trim near-black padding
    rgb = img.convert("RGB")
    black = Image.new("RGB", rgb.size, (0, 0, 0))
    diff = ImageChops.difference(rgb, black)
    def thresh(p): return 0 if p <= near_black_tol else 255
    r, g, b = diff.split()
    r = r.point(thresh); g = g.point(thresh); b = b.point(thresh)
    non_black = ImageChops.lighter(ImageChops.lighter(r, g), b)
    nb_mask = ImageChops.multiply(non_black.convert("L"), alpha)
    bbox2 = nb_mask.getbbox()
    if bbox2:
        img = img.crop(bbox2)
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

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sport", default="football", help="e.g., football, baseball")
    ap.add_argument("--league", default="nfl", help="e.g., nfl, mlb, college-football")
    ap.add_argument("--teams", default="", help="Comma list (e.g., BAL,KC,NEB,NYY). Empty=all.")
    ap.add_argument("--out", default="logos_bmp", help="Output directory")
    ap.add_argument("--height", type=int, default=32, help="Target pixel height")
    ap.add_argument("--max-width", type=int, default=48, help="Cap final width (px)")
    ap.add_argument("--black-tol", type=int, default=8, help="Near-black trim tolerance")
    ap.add_argument("--bpp", type=int, default=24, choices=[24,32], help="BMP bits per pixel")
    ap.add_argument("--force-square", action="store_true",
                    help="Pad to height×height after resize (before width cap).")
    ap.add_argument("--left-align", action="store_true",
                    help="Left-align when stride-padding (default center).")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    # Fetch teams list once
    teams_index = fetch_teams_index(args.sport, args.league)

    # Build filter set of requested abbreviations (upper-cased for comparison)
    filter_set = {t.strip().upper() for t in args.teams.split(",") if t.strip()} if args.teams else None

    for team in iter_team_objs(teams_index):
        abbr = (team.get("abbreviation") or "").upper()
        if not abbr:
            continue
        if filter_set and abbr not in filter_set:
            continue

        try:
            logo_url = pick_smallest_logo_url(team)
            if not logo_url:
                print(f"✗ {abbr} no logos[] URL found; skipping")
                continue

            img = fetch_image(logo_url)
            img = crop_transparent_and_near_black(img, near_black_tol=args.black_tol)
            img = resize_to_height(img, target_h=args.height)

            if args.force_square:
                # center into exact HxH canvas before final width cap
                w, h = img.size
                canvas = Image.new("RGBA", (args.height, args.height), (0,0,0,0))
                canvas.paste(img, ((args.height - w)//2, 0))
                img = canvas

            # Cap the final width
            img = cap_final_width(img, max_w=args.max_width)

            # Ensure stride-safe width for BMP
            img = pad_width_for_stride(img, bpp=args.bpp, center=not args.left_align)

            # Convert to BMP target mode
            out_img = remove_alpha_to_rgb(img) if args.bpp == 24 else img.convert("RGBA")

            out_path = os.path.join(args.out, f"{abbr}.bmp")
            out_img.save(out_path, format="BMP")
            print(f"✓ {abbr} -> {out_path} ({out_img.size[0]}x{out_img.size[1]}, {args.bpp}bpp)")
        except Exception as e:
            print(f"✗ {abbr} failed: {e}")

if __name__ == "__main__":
    main()
