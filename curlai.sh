#!/bin/bash

API_KEY="0492ca33-3aa0-4b4c-9dc7-7e4fcfb0968a"  # or use hardcoded: API_KEY="your_api_key_here"

curl -X 'POST' 'https://api.llmapi.com/chat/completions' \
  -H 'accept: */*' \
  -H "Authorization: Bearer $API_KEY" \
  -H 'Content-Type: application/json' \
  -d @- <<EOF
{
  "model": "llama3.1-8b",
  "messages": [
    {
      "role": "system",
      "content": "The user gives you the word in nepali but in English script. don't do anything but writing it in nepali script."
    },
    {
      "role": "user",
      "content": "$1"
    }
  ]
}
EOF

