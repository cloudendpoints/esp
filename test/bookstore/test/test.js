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
// A test for the Google Cloud Endpoints bookstore example.

var bookstore = require('../bookstore.js');
var assert = require('chai').assert;
var http = require('http');

var PORT = 8080;

function request(method, path, body, next) {
  var headers = {};
  if (body !== null) {
    headers['Content-Type'] = 'application/json';
    headers['Content-Length'] = body.length;
    headers['X-Endpoint-API-UserInfo'] = new Buffer(JSON.stringify({
      id: 'myId',
      email: 'myEmail',
      consumer_id: 'customerId'
    })).toString('base64');
  }
  var r = http.request({
    host: 'localhost',
    port: PORT,
    method: method,
    path: path,
    headers: headers,
  }, function(res) {
    var responseBody = null;
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      if (responseBody === null) {
        responseBody = chunk;
      } else {
        responseBody += chunk;
      }
    });
    res.on('end', function() {
      next(res, responseBody);
    });
  });
  if (body !== null) {
    r.write(body);
  }
  r.end();
}

function assertContentType(headers, contentType) {
  assert.property(headers, 'content-type');
  assert.isString(headers['content-type']);
  assert(
    headers['content-type'] === contentType ||
    headers['content-type'].startsWith(contentType + ';'));
}

function parseJsonBody(headers, body) {
  assertContentType(headers, 'application/json');
  return JSON.parse(body);
}

describe('bookstore', function() {
  var server;

  before(function(done) {
    // Initialize bookstore without Swagger UI and in quiet mode.
    var options = {
      log: false,
      ui: false,
    };
    server = bookstore(options).listen(PORT, '0.0.0.0', function() {
      done();
    });
  });

  after(function(done) {
    if (server) {
      server.close(done);
    } else {
      done();
    }
  });

  beforeEach(function(done) {
    // Delete all shelves.
    // In each turn we list all remaining shelves and delete one of them.
    // The algorithm terminates when an empty list of shelves is returned.
    function loop() {
      request('GET', '/shelves', null, function(res, body) {
        assert.equal(res.statusCode, 200, 'list shelves didn\'t return 200');
        var json = parseJsonBody(res.headers, body);
        var shelves = json.shelves;

        if (shelves && shelves.length > 0) {
            request('DELETE', '/' + shelves[0].name, null, function(res, body) {
              assert.equal(res.statusCode, 204, 'DELETE valid shelf didn\'t return 204');
              assert.equal(body, null);

              // Proceed deleting the next shelf.
              loop();
            });

        } else {
          // All shelves have been deleted.
          done();
        }
      });
    }
    // Initiate the deletion sequence.
    loop();
  });

  function unsupported(path, tests) {
    for (var method in tests) {
      var body = tests[method];
      it(method + ' is not supported', function(done) {
        request(method, path, body, function(res, _) {
          assert.equal(res.statusCode, 404);
          done();
        });
      });
    }
  }

  function createShelf(theme, done) {
    request('POST', '/shelves', JSON.stringify({
        theme: theme
      }), function(res, body) {
      assert.strictEqual(res.statusCode, 200);
      done(parseJsonBody(res.headers, body));
    });
  }

  function createBook(shelf, author, title, done) {
    request('POST', '/' + shelf + '/books', JSON.stringify({
        author: author,
        title: title
      }), function(res, body) {
      assert.strictEqual(res.statusCode, 200);
      done(parseJsonBody(res.headers, body));
    });
  }

  describe('/', function() {
    it('GET returns welcome page', function(done) {
      request('GET', '/', null, function(res, body) {
        assert.strictEqual(
          res.statusCode, 200,
          'fetching welcome page did not return HTTP 200');
        assertContentType(res.headers, 'text/html');
        done();
      });
    });

    unsupported('/', {
      PUT: '{}',
      POST: '{}',
      PATCH: '{}',
      DELETE: null,
    });
  });

  describe('/shelves', function() {
    var fictionShelf = null;
    var fantasyShelf = null;

    beforeEach(function(done) {
      createShelf('Fiction', function(fiction) {
        fictionShelf = fiction;
        createShelf('Fantasy', function(fantasy) {
          fantasyShelf = fantasy;
          createBook(fiction.name, 'Neal Stephenson', 'Seveneves',
                     function(seveneves) {
            createBook(fantasy.name, 'J. R. R. Tolkien',
                       'The Lord of the Rings', function(lotr) {
              done();
            });
          });
        });
      });
    });

    it('GET returns list of shelves', function(done) {
      request('GET', '/shelves', null, function(res, body) {
        assert.equal(res.statusCode, 200, 'list shelves didn\'t return 200');

        var json = parseJsonBody(res.headers, body);
        assert.property(json, 'shelves');
        assert.isArray(json.shelves);
        json.shelves.forEach(function(shelf) {
          assert.property(shelf, 'name');
          assert.property(shelf, 'theme');
          assert.isString(shelf.name);
          assert.isString(shelf.theme);
        });

        assert.sameDeepMembers(json.shelves, [fictionShelf, fantasyShelf]);

        done();
      });
    });

    it('POST creates a new shelf', function(done) {
      request('POST', '/shelves', JSON.stringify({
        theme: 'Nonfiction'
      }), function(res, body) {
        assert.equal(res.statusCode, 200, 'create shelf didn\'t return 200');

        var shelf = parseJsonBody(res.headers, body);
        assert.property(shelf, 'name');
        assert.match(shelf.name, /^shelves\/[0-9]+$/);
        assert.propertyVal(shelf, 'theme', 'Nonfiction');

        done();
      });
    });

    unsupported('/shelves', {
      PUT: '{}',
      PATCH: '{}',
      DELETE: null
    });
  });

  describe('/shelves/{shelf}', function() {
    var testShelf = null;

    beforeEach('create test shelf', function(done) {
      createShelf('Poetry', function(shelf) {
        assert.propertyVal(shelf, 'theme', 'Poetry');
        testShelf = shelf;
        done();
      });
    });

    it('GET of a valid shelf returns shelf', function(done) {
      request('GET', '/' + testShelf.name, null, function(res, body) {
        assert.equal(res.statusCode, 200, 'GET valid shelf didn\'t return 200');

        var shelf = parseJsonBody(res.headers, body);
        assert.deepEqual(shelf, testShelf);

        done();
      });
    });

    it('GET of an invalid shelf returns 404', function(done) {
      request('GET', '/shelves/999999', null, function(res, body) {
        assert.equal(res.statusCode, 404, 'GET invalid shelf didn\'t return 404');

        var error = parseJsonBody(res.headers, body);
        assert.property(error, 'message');

        done();
      });
    });

    it('DELETE of a valid shelf deletes it', function(done) {
      request('DELETE', '/' + testShelf.name, null, function(res, body) {
        assert.equal(res.statusCode, 204, 'DELETE valid shelf didn\'t return 204');
        assert.equal(body, null);
        done();
      });
    });

    it('DELETE of an invalid shelf returns 404', function(done) {
      request('DELETE', '/' + testShelf.name, null, function(res, body) {
        assert.equal(res.statusCode, 204, 'DELETE valid shelf didn\'t return 204');
        assert.equal(body, null);

        // Try to delete the same shelf again.
        request('DELETE', '/' + testShelf.name, null, function(res, body) {
          assert.equal(res.statusCode, 404, 'DELETE invalid shelf didn\'t return 404');

          var error = parseJsonBody(res.headers, body);
          assert.property(error, 'message');

          done();
        });
      });
    });

    unsupported('/shelves/1', {
      PUT: '{}',
      POST: '{}',
      PATCH: '{}',
    });
  });

  describe('/shelves/{shelf}/books', function() {
    var testShelf = null;
    var testKnuth = null;
    var testStroustrup = null;

    beforeEach('create test shelf and book', function(done) {
      createShelf('Computers', function(shelf) {
        assert.propertyVal(shelf, 'theme', 'Computers');
        testShelf = shelf;

        createBook(shelf.name, 'Donald E. Knuth',
          'The Art of Computer Programming', function(book) {
          assert.propertyVal(book, 'author', 'Donald E. Knuth');
          assert.propertyVal(book, 'title', 'The Art of Computer Programming');
          testKnuth = book;

          createBook(shelf.name, 'Bjarne Stroustrup',
            'The C++ Programming Language', function(book) {
            assert.propertyVal(book, 'author', 'Bjarne Stroustrup');
            assert.propertyVal(book, 'title', 'The C++ Programming Language');
            testStroustrup = book;

            done();
          });
        });
      });
    });

    it('GET lists books on a valid shelf', function(done) {
      request('GET', '/' + testShelf.name + '/books', null, function(res, body) {
        assert.strictEqual(res.statusCode, 200, 'List books didn\'t return 200');

        var response = parseJsonBody(res.headers, body);
        assert.property(response, 'books');
        assert.isArray(response.books);
        assert.sameDeepMembers(response.books, [testKnuth, testStroustrup]);

        done();
      });
    });

    it('GET returns 404 for an invalid shelf', function(done) {
      request('GET', '/shelves/999999/books', null, function(res, body) {
        assert.strictEqual(res.statusCode, 404);

        var error = parseJsonBody(res.headers, body);
        assert.property(error, 'message');

        done();
      });
    });

    it('POST creates a new book in a valid shelf', function(done) {
      var practice = {
        author: 'Brian W. Kernighan, Rob Pike',
        title: 'The Practice of Programming'
      };
      request('POST', '/' + testShelf.name + '/books', JSON.stringify(practice),
        function(res, body) {
          assert.strictEqual(res.statusCode, 200);

          var book = parseJsonBody(res.headers, body);

          assert.propertyVal(book, 'author', 'Brian W. Kernighan, Rob Pike');
          assert.propertyVal(book, 'title', 'The Practice of Programming');
          assert.property(book, 'name');
          assert.isString(book.name);
          assert.match(book.name, /^shelves\/[0-9]+\/books\/[0-9]+$/);

          done();
        });
    });

    it('POST returns 404 for an invalid shelf', function(done) {
      var compilers = {
        author: 'Aho, Sethi, Ullman',
        title: 'Compilers'
      };
      request('POST', '/shelves/999999/books', JSON.stringify(compilers),
        function(res, body) {
          assert.strictEqual(res.statusCode, 404);

          var error = parseJsonBody(res.headers, body);
          assert.property(error, 'message');

          done();
        });
    });

    unsupported('/shelves/1/books', {
      PUT: '{}',
      PATCH: '{}',
      DELETE: null
    });
  });

  describe('/shelves/{shelf}/books/{book}', function() {
    var testYoga = null;
    var testSutras = null;
    var testBreathing = null;

    beforeEach('create test shelf and books', function(done) {
      createShelf('Yoga', function(shelf) {
        assert.propertyVal(shelf, 'theme', 'Yoga');
        testYoga = shelf;

        createBook(shelf.name, 'Patanjali', 'Yoga Sutras of Patanjali', function(book) {
          assert.propertyVal(book, 'author', 'Patanjali');
          assert.propertyVal(book, 'title', 'Yoga Sutras of Patanjali');
          testSutras = book;

          createBook(shelf.name, 'Donna Farhi', 'The breathing book', function(book) {
            assert.propertyVal(book, 'author', 'Donna Farhi');
            assert.propertyVal(book, 'title', 'The breathing book');
            testBreathing = book;

            done();
          });
        });
      });
    });

    it('GET of a valid book returns a book', function(done) {
      request('GET', '/' + testBreathing.name, null, function(res, body) {
        assert.strictEqual(res.statusCode, 200);

        var book = parseJsonBody(res.headers, body);
        assert.deepEqual(book, testBreathing);

        done();
      });
    });

    it('GET of a book on an invalid shelf returns 404', function(done) {
      request('GET', '/shelves/999999/books/5', null, function(res, body) {
        assert.strictEqual(res.statusCode, 404);

        var error = parseJsonBody(res.headers, body);
        assert.property(error, 'message');

        done();
      });
    });

    it('GET of an invalid book on valid shelf returns 404', function(done) {
      request('GET', '/' + testYoga.name + '/books/999999', null, function(res, body) {
        assert.strictEqual(res.statusCode, 404);

        var error = parseJsonBody(res.headers, body);
        assert.property(error, 'message');

        done();
      });
    });

    it('DELETE of a valid book deletes the book', function(done) {
      request('DELETE', '/' + testSutras.name, null, function(res, body) {
        assert.strictEqual(res.statusCode, 204);
        assert.equal(body, null);
        done();
      });
    });

    it('DELETE of a book on an invalid shelf returns 404', function(done) {
      request('DELETE', '/shelves/999999/books/5', null, function(res, body) {
        assert.strictEqual(res.statusCode, 404);

        var error = parseJsonBody(res.headers, body);
        assert.property(error, 'message');

        done();
      });
    });

    it('DELETE of an invalid book on a valid shelf returns 404', function(done) {
      // Delete the book as above.
      request('DELETE', '/' + testSutras.name, null, function(res, body) {
        assert.strictEqual(res.statusCode, 204);
        assert.equal(body, null);
        // Delete the same book again.
        request('DELETE', '/' + testSutras.name, null, function(res, body) {
          assert.strictEqual(res.statusCode, 404);

          var error = parseJsonBody(res.headers, body);
          assert.property(error, 'message');

          done();
        });
      });
    });

    unsupported('/shelves/1/books/2', {
      PUT: '{}',
      POST: '{}',
      PATCH: '{}',
    });
  });

  describe ('/version', function() {
    it('GET returns version', function(done) {
      request('GET', '/version', null, function(res, body) {
        assert.equal(res.statusCode, 200, '/version didn\'t return 200');

        var json = parseJsonBody(res.headers, body);
        assert.property(json, 'version');
        assert.isString(json.version);

        done();
      });
    });
  });
});

describe('bookstore-ui', function() {
  var server;

  before(function(done) {
    var swagger = require('../swagger.json');
    swagger.host = 'localhost:8080';

    // Initialize bookstore with Swagger UI and in quiet mode.
    var options = {
      log: false,
      swagger: swagger,
    };

    server = bookstore(options).listen(PORT, '0.0.0.0', function() {
      done();
    });
  });

  after(function(done) {
    if (server) {
      server.close(done);
    } else {
      done();
    }
  });

  describe('/docs', function() {
    it('GET on Swagger UI works', function(done) {
      request('GET', '/docs/', null, function(res, body) {
        assert.strictEqual(res.statusCode, 200,
                           'fetching Swagger UI did not return HTTP 200');
        done();
      });
    });
  });

  describe('/api-docs', function() {
    it('GET returns swagger.json', function(done) {
      request('GET', '/api-docs', null, function(res, body) {
        assert.strictEqual(
            res.statusCode, 200, 'fetching swagger did not return HTTP 200');

        var swagger = parseJsonBody(res.headers, body);
        assert.propertyVal(swagger, 'swagger', '2.0');
        assert.deepPropertyVal(swagger, 'info.title', 'Bookstore');

        done();
      });
    });
  });
});
