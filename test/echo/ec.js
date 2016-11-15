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

var parallel = 10;

var http = require('http');
var data = "\
DUKE. ...  \n\
  Will give thee time to leave our royal court, \n\
  By heaven! my wrath shall far exceed the love \n\
  I ever bore my daughter or thyself. \n\
  Be gone; I will not hear thy vain excuse, \n\
  But, as thou lov'st thy life, make speed from hence.    Exit \n\
VALENTINE. And why not death rather than living torment? \n\
  To die is to be banish'd from myself, \n\
  And Silvia is myself; banish'd from her \n\
  Is self from self, a deadly banishment. \n\
  What light is light, if Silvia be not seen? \n\
  What joy is joy, if Silvia be not by? \n\
  Unless it be to think that she is by, \n\
  And feed upon the shadow of perfection. \n\
  Except I be by Silvia in the night, \n\
  There is no music in the nightingale; \n\
  Unless I look on Silvia in the day, \n\
  There is no day for me to look upon. \n\
  She is my essence, and I leave to be \n\
  If I be not by her fair influence \n\
  Foster'd, illumin'd, cherish'd, kept alive. \n\
  I fly not death, to fly his deadly doom: \n\
  Tarry I here, I but attend on death; \n\
  But fly I hence, I fly away from life. \n\
";

var totalRequests = 0;
var totalData = 0;

var port = process.env.PORT || 8080;

function one() {
  var times = 1;

  var req = http.request({
    port: port,
    method: 'POST',
    headers: {
      'Content-Length': data.length * times,
    }
  }, function(res) {
    res.on('data', function(chunk) {
      totalData += chunk.length;
    });

    res.on('end', function(){
      totalRequests += 1;
      setImmediate(one);
    });

  })

  for (var i = 0 ; i < times ; i ++) {
    req.write(data);
  }
  req.end();
}

setInterval(function() {
  console.log("Requests:", totalRequests, " Data: ", totalData);
}, 1000);

for (var i = 0; i < parallel; i ++) {
  setImmediate(one);
}


