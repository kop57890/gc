#!/usr/bin/python3.5
# -*- coding: UTF-8 -*-
import json, sys, os
os.system('play -t raw -r 16k -e signed -b 16 -c 1 tts_sample.pcm')
# jsondata = str(sys.argv[1]).strip()
# data = json.loads(jsondata) 
# if "answer" in data["intent"]:    
#     print(data["intent"]["answer"]["text"])
#     os.system('./tts_sample ' + data["intent"]["answer"]["text"])
#     os.system('play -t raw -r 16k -e signed -b 16 -c 1 tts_sample.pcm')
# else:
#     print("Please try again.")