import displayio
import terminalio
from adafruit_display_text import label
from adafruit_bitmap_font import bitmap_font

# Write your code here :-)
class SportsDisplay:
    def __init__(self, display, sport, league):
        # Load the Tom Thumb font
        self.small_font = bitmap_font.load_font("/fonts/tom-thumb.bdf")

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

        # Scores
        self.away_score = label.Label(terminalio.FONT, text="", color=0xFFFF00, x=36, y=15)
        self.main_group.append(self.away_score)
        self.home_score = label.Label(terminalio.FONT, text="", color=0xFFFF00, x=64, y=15)
        self.main_group.append(self.home_score)

        # Game Date and Time
        self.game_date = label.Label(terminalio.FONT, text="", color=0xFFFFFF, x=48, y=15)
        self.main_group.append(self.game_date)
        self.game_time = label.Label(terminalio.FONT, text="", color=0xFFFFFF, x=48, y=27)
        self.main_group.append(self.game_time)

        # Game Status
        self.inning = label.Label(self.small_font, text="", color=0xFFFF00, x=48, y=5)
        self.main_group.append(self.inning)
        self.inning_logo = displayio.Group(scale=1, x=60, y=2)
        self.main_group.append(self.inning_logo)

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
            print(f"Logo error for {abbr}: {e}")

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
        if competition.state == "pre":
            self.center_text(self.game_date, competition.date, 32, 96)
            self.center_text(self.game_time, competition.time, 32, 96)
        elif competition.state == "post":
            self.center_text(self.game_time, competition.shortDetail, 32, 96)
        elif competition.state == "in":
            self.inning.text = competition.inning
            if competition.shortDetail.find("Top"):
                self.load_image(self.inning_logo, "images/top.bmp")
                self.inning_logo.y = 2
            else:
                self.load_image(self.inning_logo, "images/bottom.bmp")
                self.inning_logo.y=0
            self.inning_logo.x = self.center_label_and_image(self.inning, 7, 32, 96, spacing=1)

        # Set the State of Information
        self.away_score.hidden = (competition.state == "pre")
        self.home_score.hidden = (competition.state == "pre")
        self.game_date.hidden = (competition.state != "pre")
        self.game_time.hidden = (competition.state == "in")
        self.inning_logo.hidden = (competition.state != "in")
        self.inning.hidden = (competition.state != "in")

        return
