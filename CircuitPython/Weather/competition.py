from team import Team

# Write your code here :-)
class Competition:
    def __init__(self):
        # Generic Data
        self.shortDetail = 'NA'
        self.state = 'NA'
        self.time = 0
        self.date = 0
        self.period = 1
        self.clock = ''
        self.away_team = Team()
        self.home_team = Team()

        # Baseball Data
        self.inning = '1'
        self.outs = '0 Outs'
        self.on_first = False
        self.on_second = False
        self.on_third = True

        # Football Data
        self.yard_line = None
        self.possession_team = ''
        self.down_dist = ''


