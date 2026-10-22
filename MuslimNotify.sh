#!/bin/bash
# MuslimNotify - Prayer Time Notifier ✨

# Fetch prayer times for Sarajevo, Bosnia
LOCATION="Sarajevo"
COUNTRY="Bosnia and Herzegovina"
PRAYER_DATA=$(curl -s "https://api.aladhan.com/v1/timingsByCity?city=$LOCATION&country=$COUNTRY&method=8")

# Extract prayer times
FAJR=$(echo "$PRAYER_DATA" | jq -r '.data.timings.Fajr')
DHUHR=$(echo "$PRAYER_DATA" | jq -r '.data.timings.Dhuhr')
ASR=$(echo "$PRAYER_DATA" | jq -r '.data.timings.Asr')
MAGHRIB=$(echo "$PRAYER_DATA" | jq -r '.data.timings.Maghrib')
ISHA=$(echo "$PRAYER_DATA" | jq -r '.data.timings.Isha')

# Adhan audio (Moayyad Hakeem - YouTube)
ADHAN_URL="https://www.youtube.com/watch?v=2SR2bqYk-xk"

# Play adhan (requires youtube-dl + mpv or vlc)
play_adhan() {
  if command -v mpv &> /dev/null; then
    mpv --no-video --really-quiet "$ADHAN_URL" &
  elif command -v vlc &> /dev/null; then
    vlc --play-and-exit --no-video "$ADHAN_URL" &
  else
    echo "⚠️ Install 'mpv' or 'vlc' to play adhan audio."
  fi
}

# Notify and play adhan
notify_and_play() {
  notify-send "🕋 MuslimNotify" "$1 Prayer Time is NOW! 🕋"
  play_adhan
  echo "$(date +'%H:%M') - $1 Prayer Time!" >> ~/MuslimNotify/log.txt
}

# Schedule notifications
while true; do
  CURRENT_HOUR=$(date +'%H:%M')
  [[ "$CURRENT_HOUR" == "$FAJR" ]] && notify_and_play "Fajr"
  [[ "$CURRENT_HOUR" == "$DHUHR" ]] && notify_and_play "Dhuhr"
  [[ "$CURRENT_HOUR" == "$ASR" ]] && notify_and_play "Asr"
  [[ "$CURRENT_HOUR" == "$MAGHRIB" ]] && notify_and_play "Maghrib"
  [[ "$CURRENT_HOUR" == "$ISHA" ]] && notify_and_play "Isha"
  sleep 60
done
