# Write your code here :-)
class Competition:
    def __init__(self):
        self.home_team_abbr = 'NA'
        self.home_team_score = 0
        self.away_team_abbr = 'NA'
        self.away_team_score = 0
        self.shortDetail = 'NA'
        self.state = 'NA'
        self.time = 0
        self.date = 0

    def bark(self):
        print(f"{self.name} says woof!")

    def info(self):
        return f"{self.name} is a {self.breed}"
