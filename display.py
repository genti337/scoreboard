import displayio
import terminalio
from adafruit_display_text import label
from adafruit_bitmap_font import bitmap_font
from collections import OrderedDict

# Write your code here :-)
class SportsDisplay:
    def __init__(self, display, sport, league):
        # Load the Tom Thumb font
        self.small_font = bitmap_font.load_font("/fonts/04B_03__6pt.pcf")

        # Sport and League
        self.sport = sport
        self.league = league

        # Display Group
        self.main_group = displayio.Group()
        display.root_group = self.main_group

        # Logo groups
        self.away_team_logo = displayio.Group(scale=1, x=0, y=0)
        self.main_group.append(self.away_team_logo)
        self.home_team_logo = displayio.Group(scale=1, x=96, y=0)
        self.main_group.append(self.home_team_logo)

        # Team Abbreviations
        self.away_team_abbr = label.Label(terminalio.FONT, text="", color=0xFFFFFF, x=36, y=5)
        self.main_group.append(self.away_team_abbr)
        self.home_team_abbr = label.Label(terminalio.FONT, text="", color=0xFFFFFF, x=64, y=5)
        self.main_group.append(self.home_team_abbr)

        # Team Ranks
        self.away_team_rank = label.Label(self.small_font, text="", color=0xFFFFFF, x=32, y=20)
        self.main_group.append(self.away_team_rank)
        self.home_team_rank = label.Label(self.small_font, text="", color=0xFFFFFF, x=84, y=20)
        self.main_group.append(self.home_team_rank)

        # Team Record
        self.away_team_record = label.Label(self.small_font, text="", color=0xFFFFFF, x=36, y=12)
        self.main_group.append(self.away_team_record)
        self.home_team_record = label.Label(self.small_font, text="", color=0xFFFFFF, x=36, y=12)
        self.main_group.append(self.home_team_record)

        # Scores
        self.away_score = label.Label(terminalio.FONT, text="", color=0xFFFF00, x=36, y=15)
        self.main_group.append(self.away_score)
        self.home_score = label.Label(terminalio.FONT, text="", color=0xFFFF00, x=64, y=15)
        self.main_group.append(self.home_score)

        # Game Date and Time
        self.game_date = label.Label(self.small_font, text="", color=0xFFFFFF, x=48, y=20)
        self.main_group.append(self.game_date)
        self.game_time = label.Label(self.small_font, text="", color=0xFFFFFF, x=48, y=27)
        self.main_group.append(self.game_time)
        self.game_status = label.Label(self.small_font, text="", color=0xFFFFFF, x=48, y=15)
        self.main_group.append(self.game_status)

        # Baseball Game Status
        self.inning = label.Label(self.small_font, text="", color=0xFFFF00, x=48, y=19)
        self.main_group.append(self.inning)
        self.inning_logo = displayio.Group(scale=1, x=60, y=27)
        self.main_group.append(self.inning_logo)
        self.outs = label.Label(self.small_font, text="", color=0xFFFFFF, x=48, y=27)
        self.main_group.append(self.outs)
        self.first_base = displayio.Group(scale=1, x=66, y=9)
        self.main_group.append(self.first_base)
        self.second_base = displayio.Group(scale=1, x=60, y=3)
        self.main_group.append(self.second_base)
        self.third_base = displayio.Group(scale=1, x=54, y=9)
        self.main_group.append(self.third_base)

        # Ordered Dictionary for Bases Loaded State
        self.bases_image_dict = OrderedDict()
        self.bases_image_dict[True] = "images/base_loaded.bmp"
        self.bases_image_dict[False] = "images/base_empty.bmp"

        # Football Game Status
        self.football_field = displayio.Group(scale=1, x=32, y=24)
        self.main_group.append(self.football_field)
        self.football = displayio.Group(scale=1, x=96, y=27)
        self.main_group.append(self.football)

    # Load a 32x32 BMP logo
    def load_logo(self, group, abbr):
        while len(group) > 0:
            group.pop()
        try:
            filename = f"/images/{self.league}/{abbr.lower()}.bmp"
            bitmap = displayio.OnDiskBitmap(open(filename, "rb"))
            tile_grid = displayio.TileGrid(bitmap, pixel_shader=bitmap.pixel_shader)
            group.append(tile_grid)
        except Exception as e:
            print(f"Logo error for {abbr}: {e}")

    # Load a 32x32 BMP logo
    def load_image(self, group, filename):
        while len(group) > 0:
            group.pop()
        try:
            bitmap = displayio.OnDiskBitmap(open(filename, "rb"))
            tile_grid = displayio.TileGrid(bitmap, pixel_shader=bitmap.pixel_shader)
            group.append(tile_grid)
        except Exception as e:
            print(f"Logo error for {filename}: {e}")

    def center_text(self, label, text, min_x, max_x):
        '''
        Center a label horizontally between min_x and max_x.

        Parameters:
            label (Label): The displayio or adafruit_display_text label.
            text (string): Label Text
            min_x (int): Minimum x position of the area to center in.
            max_x (int): Maximum x position of the area to center in.
        '''

        label.text = text
        x, y, width, height = label.bounding_box
        label.x = min_x + (max_x - min_x - width) // 2

        return

    def center_image(self, image_width, min_x, max_x):
        '''
        Center a label horizontally between min_x and max_x.

        Parameters:
            label (Label): The displayio or adafruit_display_text label.
            text (string): Label Text
            min_x (int): Minimum x position of the area to center in.
            max_x (int): Maximum x position of the area to center in.
        '''

        image_x = min_x + (max_x - min_x - image_width) // 2

        return image_x

    def center_label_and_image(self, label, image_width, min_x, max_x, spacing=0):
        """
        Centers a label and an image that appears immediately to its right.

        Parameters:
            label (Label): adafruit_display_text.label.Label object to center.
            image_width (int): Width of the bitmap/image in pixels.
            min_x (int): Minimum x boundary.
            max_x (int): Maximum x boundary.
            spacing (int): Pixels between label and image.

        Returns:
            int: x position to place the image.
        """
        # Get label width
        if hasattr(label, "bounding_box"):
            label_width = label.bounding_box[2]
        else:
            label_width = len(label.text) * 6  # rough fallback

        total_width = label_width + spacing + image_width
        start_x = min_x + (max_x - min_x - total_width) // 2

        image_x = start_x
        label.x = start_x + image_width + spacing

        return image_x

    def align_text_right(self, target_x, text, font):
        width = sum(font.get_glyph(ord(c)).shift_x for c in text if font.get_glyph(ord(c)))

        return target_x - width


    def yardline_to_field_position(self, yardline_str, possession_team, home_team, away_team):
        """
        Converts ESPN 'yardLine' (e.g., "DAL 42") to 0–100 field position.
        0 = left end zone, 100 = right end zone.
        """
        if not yardline_str or ' ' not in yardline_str:
            return None

        team_code, yard = yardline_str.split()
        yard = int(yard)

        # Determine direction of play
        # Assume possession team is driving toward opponent's end zone
        # Home team is on the right side of the field
        if possession_team == away_team:
            # Possession going right
            if team_code == away_team:
                x = 100 - yard
            else:
                x = yard
        else:
            # Possession going left
            if team_code == home_team:
                x = 100 - yard
            else:
                x = yard

        return x

    def update(self, competition):
        print("Updating Display!")

        # Update Team Logos
        self.load_logo(self.away_team_logo, competition.away_team.abbr)
        self.load_logo(self.home_team_logo, competition.home_team.abbr)

        # Update Team Abbreviations
        self.center_text(self.away_team_abbr, competition.away_team.abbr, 36, 52)
        self.center_text(self.home_team_abbr, competition.home_team.abbr, 76, 92)

        # Update Team Scores
        self.center_text(self.away_score, competition.away_team.score, 36, 52)
        self.center_text(self.home_score, competition.home_team.score, 76, 92)

        # Pre-Game Information
        #competition.state = "in"
        if competition.state == "pre":
            self.center_text(self.game_date, competition.date, 32, 96)
            self.center_text(self.game_time, competition.time, 32, 96)
            self.center_text(self.away_team_record, competition.away_team.record, 32, 64)
            self.center_text(self.home_team_record, competition.home_team.record, 64, 96)
            self.center_text(self.away_team_rank, competition.away_team.rank, 32, 48)
            self.center_text(self.home_team_rank, competition.home_team.rank, 80, 96)
        elif competition.state == "post":
            self.center_text(self.game_status, "Final", 32, 96)
            self.center_text(self.away_team_record, competition.away_team.record, 32, 64)
            #self.center_text(self.home_team_record, competition.home_team.record, 64, 96)
        elif competition.state == "in":
            if self.sport == "baseball":
                self.inning.text = competition.inning
                self.outs.text = competition.outs
                if competition.shortDetail.find("Top"):
                    self.load_image(self.inning_logo, "images/top.bmp")
                    self.inning_logo.y = 18
                    self.outs.x = 31
                else:
                    self.load_image(self.inning_logo, "images/bottom.bmp")
                    self.inning_logo.y = 15
                    self.center_text(self.outs, competition.outs, 64, 96)
                    self.outs.x = self.align_text_right(96, self.outs.text, self.small_font)
                    print(self.outs.x)
                self.inning_logo.x = self.center_label_and_image(self.inning, 7, 32, 96, spacing=1)

                self.load_image(self.first_base, self.bases_image_dict[competition.on_first])
                self.load_image(self.second_base, self.bases_image_dict[competition.on_second])
                self.load_image(self.third_base, self.bases_image_dict[competition.on_third])
            elif self.sport == "football":
                yardline = self.yardline_to_field_position("MIC 30", "MIC", "MIC", "HOU")
                print(yardline)

                self.football.x = int(34 + (92 - 36) * (yardline / 100))
                self.load_image(self.football_field, "images/football_field.bmp")
                self.load_image(self.football, "images/football.bmp")

        # Set the State of Information
        self.away_score.hidden = (competition.state == "pre")
        self.home_score.hidden = (competition.state == "pre")
        self.game_date.hidden = (competition.state != "pre")
        self.game_time.hidden = (competition.state == "in")
        self.inning_logo.hidden = (competition.state != "in")
        self.inning.hidden = (competition.state != "in")
        self.first_base.hidden = (competition.state != "in")
        self.second_base.hidden = (competition.state != "in")
        self.third_base.hidden = (competition.state != "in")
        self.outs.hidden = (competition.state != "in")
        self.home_team_rank.hidden = (competition.state == "in")
        self.away_team_rank.hidden = (competition.state == "in")
        self.game_status.hidden = (competition.state != "post")
        self.away_team_record.hidden = (competition.state == "in")
        self.home_team_record.hidden = (competition.state == "in")

        return
