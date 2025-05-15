from team import Team

# Write your code here :-)
class Competition:
    def __init__(self):
        # Generic Data
        self.shortDetail = 'NA'
        self.state = 'NA'
        self.time = 0
        self.date = 0
        self.away_team = Team()
        self.home_team = Team()

        # Baseball Data
        self.inning = '1'
        self.outs = '0'
        self.on_first = 'No'
        self.on_second = 'No'
        self.on_third = 'No'


