pulseaudio -D
pacmd load-module module-null-sink sink_name=MySink
Xvfb :1 -screen 0 1024x768x24 +extension GLX +render -noreset >> xsession.log 2>&1 &