#!/bin/bash


curl "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=AIzaSyCqNU__RMkzTbgZ-Ful6AB1OoZ2lci3Vtg" \
  -H 'Content-Type: application/json' \
  -X POST \
    -d '{
    "contents": [
      {
        "role": "user",
        "parts": [
          {
            "text": "Hello, can you remember that my name is Meowrajan "
          }
        ]
      },
      {
        "role": "model",
        "parts": [
          {
            "text": "okay sure"
          }
        ]
      },
      {
        "role": "user",
        "parts": [
          {
            "text": "tell me my name real quick"
          }
        ]
      }
    ]
  }'

