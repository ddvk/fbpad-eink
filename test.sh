make || exit 1
ssh rm "killall fbpad"
scp fbpad rm:
ssh rm "./fbpad bash"
