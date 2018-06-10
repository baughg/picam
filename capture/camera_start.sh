cd /home/gary/controller/
sudo ./bfan off
sleep 5
sudo ./bfan
sleep 3
sudo ptpcam --chdk='mode 1'
sleep 2
sudo ptpcam --chdk="lua set_mf(1)"
sleep 2
#sudo ptpcam --chdk="lua set_aflock(1)"
sleep 2
sudo ptpcam --chdk="lua set_focus(30000)"
sleep 5
#sudo ptpcam --chdk="lua set_mf(1)"
#sleep 2
cd /home/gary/capture/
lsusb | grep 04a9:3244
