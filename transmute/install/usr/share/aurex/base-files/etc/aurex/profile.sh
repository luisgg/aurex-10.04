# system-wide .profile file for AureX scripts

for d in /usr/share/aurex/profile.d /etc/aurex/profile.d ; do
	if [ -d "$d" ]; then
		for f in $d/*.sh; do
			[ ! -r "$f" ] || . $f
		done
		unset f
	fi
done
unset d
