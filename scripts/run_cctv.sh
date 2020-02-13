sudo mount -a
/home/gary/capture/camera_start.sh
screen -dmS webcam bash -c 'echo "starting webcam system...";cd /home/gary/webcam;sudo python webcam.py;exec bash'
screen -dmS capture bash -c 'echo "starting capture system...";cd /home/gary/capture;sudo python capture_server.py;exec bash'
