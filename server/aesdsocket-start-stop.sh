#! /bin/sh
case "$1" in
  start)
    echo "Starting AESD Socket application"
    start-stop-daemon aesdsocket -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    ;;
  stop)
    echo "Stopping AESD Socket application"
    start-stop-daemon aesdsocket -K -n aesdsocket -a /usr/bin/aesdsocket
    ;;
  *)
    echo "USAGE:: start|stop"
  exit 1
esac
exit 0
    
