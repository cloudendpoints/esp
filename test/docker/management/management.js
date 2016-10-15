// Copyright (C) Endpoints Server Proxy Authors
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
var fs = require('fs');

function management() {
  var management = express();

  // Tracing middleware.
  management.use(function(req, res, next) {
    console.log(req.method, req.originalUrl);
    console.log(req.headers);
    console.log();
    next();
  })

  management.get('/', function(req, res) {
    res.send(process.env.ACCESS_TOKEN);
  });

  var content = fs.readFileSync('service.json');
  var json = JSON.parse(content);

  // Set a custom service control
  json.control.environment = process.env.CONTROL_URL;

  management.get("/service_config", function(req, res) {
    if (req.param.configId == json.id) {
      res.send('Incorrect version');
      return;
    }

    var auth = req.headers.authorization;

    if (auth != 'Bearer ' + process.env.ACCESS_TOKEN) {
      res.send('Incorrect access token');
      return;
    }

    res.send(JSON.stringify(json));
  });

  return management;
}

if (module.parent) {
  module.exports = management;
} else {
  var server = management().listen(process.env.MANAGEMENT_PORT || '8080', '0.0.0.0', function() {
    var host = server.address().address;
    var port = server.address().port;

    console.log('Management listening at http://%s:%s', host, port);
  })
}
