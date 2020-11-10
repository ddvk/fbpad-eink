make || exit 1
ssh rm "killall fbpad"
scp fbpad rm:
ssh -t rm "./fbpad bash"
