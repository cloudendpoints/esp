# Copyright (C) Extensible Service Proxy Authors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

set key off
set xlabel "# Parallel Calls"
set term png size 1200, 800
set xrange [0:1010]
set ylabel "QPS"

set title "GRPC Proxy QPS Scalability"
set output "grpc-qps.png"
plot "grpc.dat" using 1:2:3 with errorbars t "QPS", \
     "" using 1:2:3 smooth csplines t "QPS Smoothed"

set title "Raw QPS Scalability"
set output "raw-qps.png"
plot "raw.dat" using 1:2:3 with errorbars t "QPS", \
     "" using 1:2:3 smooth csplines t "QPS Smoothed"

set ylabel "Latency (ms)"

set title "GRPC Proxy Latency Scalability"
set output "grpc-latency.png"
plot "grpc.dat" using 1:4:5 with errorbars t "Latency", \
     "" using 1:4:5 smooth csplines t "Latency Smoothed"

set title "Raw Latency Scalability"
set output "raw-latency.png"
plot "raw.dat" using 1:4:5 with errorbars t "Latency", \
     "" using 1:4:5 smooth csplines t "Latency Smoothed"
