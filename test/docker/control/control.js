// Copyright (C) Extensible Service Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//

"use strict";

var express = require('express');

function control() {
  var control = express();

  // Tracing middleware.
  control.use(function(req, res, next) {
    console.log(req.method, req.originalUrl);
    console.log(req.headers);
    console.log();
    next();
  })

  control.get('/', function(req, res) {
    res.send(process.env.ACCESS_TOKEN);
  });

  control.get(/^\/v1\/services\/([A-Za-z0-9_.-]+):check$/, function(req, res) {
    res.status(200).end();
  });

  control.post(/^\/v1\/services\/([A-Za-z0-9_.-]+):check$/, function(req, res) {
    res.status(200).end();
  });

  control.get(/^\/v1\/services\/([A-Za-z0-9_.-]+):report$/, function(req, res) {
    res.status(200).end();
  });

  control.post(/^\/v1\/services\/([A-Za-z0-9_.-]+):report$/, function(req, res) {
    res.status(200).end();
  });

  return control;
}

if (module.parent) {
  module.exports = control;
} else {
  var server = control().listen(process.env.CONTROL_PORT || '8080', '0.0.0.0', function() {
    var host = server.address().address;
    var port = server.address().port;

    console.log('Metadata listening at http://%s:%s', host, port);
  })
}
