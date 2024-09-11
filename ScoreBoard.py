import requests
import time

def get_events(sport, league):
   sport_name = "football"
   sport_league = "college-football"
   url = "https://site.api.espn.com/apis/site/v2/sports/%s/%s/scoreboard" % (sport, league)

   # Make the request to the API
   resp = requests.get(url)
   
   # Stream to json
   data = resp.json()

   # Create list of events
   event_ids = []
   for event in data['events']:
      event_ids.append(event['id'])

   return event_ids

def get_event_data(sport, league, event_id):

   sport_name = "football"
   sport_league = "college-football"
   url = 'http://site.api.espn.com/apis/site/v2/sports/football/college-football/summary?event=%s' % (event_id)

   # Make the request to the API
   resp = requests.get(url)
   
   # Stream to json
   data = resp.json()

#   print("%s @ %s" % (data['boxscore']['teams'][0]['team']['displayName'], data['boxscore']['teams'][1]['team']['displayName']))
#   print(data['boxscore']['teams'][0])

#   print(data.keys())
#   print(data['header'])
#   print(data['header']['competitions'])
#   for competitor in data['header']["competitors"]:
#      print(competitor["team"]["abbreviation"])
#      print(competitor["team"]["logo"])
#      print(competitor["score"])

   for competition in data['header']["competitions"]:
      for competitor in competition["competitors"]:
         print(competitor)
         print("\n")
#         print(competitor["team"]["abbreviation"])
#         print(competitor["team"]["logo"])
#         print(competitor["score"])
#


#   # Loop through events
#   for event in data['events']:
#      if event_name == event['name']:
#         print(event_name)
#         print(event["status"]["type"]["shortDetail"])
#         for competition in event["competitions"]:
#            for competitor in competition["competitors"]:
#               print(competitor["team"]["abbreviation"])
#               print(competitor["team"]["logo"])
#               print(competitor["score"])

def get_team_data(team):
   url = "https://site.api.espn.com/apis/site/v2/sports/football/college-football/teams/%s" % (team)

   # Make the request to the API
   resp = requests.get(url)
   
   # Stream to json
   data = resp.json()

   print(data['team']['abbreviation'])
   print(data['team']['nextEvent'][0]['name'])
   print(data['team']['nextEvent'][0]['date'])
   print(data['team']['nextEvent'][0]['competitions'][0]['competitors'][0]['team']['abbreviation'])
   print(data['team']['nextEvent'][0]['competitions'][0]['competitors'][1]['team']['abbreviation'])
   print(data['team']['nextEvent'][0]['competitions'][0]['competitors'][0]['score'])

def get_rankings():
   url = 'https://site.api.espn.com/apis/site/v2/sports/football/college-football/rankings'

   # Make the request to the API
   resp = requests.get(url)
   
   # Stream to json
   data = resp.json()

   print(data.keys())

   for rankings in data['rankings']:
      for rank in rankings['ranks']:
         print(rank['current'])


# Retrieve all Events
sport = "football"
league = "college-football"
event_ids = get_events(sport, league)

#index = 0
#while True:
#   get_event_data(sport, league, event_ids[index])
#
#   # Increment Event Index
#   if index >= len(event_ids) - 1:
#      index = 0
#   else:
#      index = index + 1
#
#   time.sleep(1.0)
#
#   break

#print(event_ids)
#get_event_data(sport, league, event_ids[0])

get_team_data("nebraska")
