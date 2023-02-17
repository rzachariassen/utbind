;
; type	domain	source file or host
;
; see man page named.8 in the man directory
; /etc/nsdata is directory from where files are relative from.
directory /etc/nsdata
sortlist	128.100.9.0 128.100.8.0
;
; load control.toronto.edu domain from /etc/nsdata/domain/control.toronto.edu
primary		control.toronto.edu	domain/control.toronto.edu
; load control.utoronto.ca domain from /etc/nsdata/domain/control.toronto.edu
primary		control.utoronto.ca	domain/control.utoronto.ca
; load cache data
cache		.			named.ca
; load the reverse address mapping
primary		9.100.128.in-addr.arpa	ptrprimary/control-ether
primary		0.0.127.in-addr.arpa	named.local
forwarders	128.100.8.1 128.100.1.66
