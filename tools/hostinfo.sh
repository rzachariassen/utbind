#!/bin/sh -
nslookup << EOF | sed -e '/^>/d' -e '/^Address/d' -e '/^Defaul/d' -e '/^$/d'
set defname
set querytype=ANY
$1
EOF
