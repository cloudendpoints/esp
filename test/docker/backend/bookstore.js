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
// A Google Cloud Endpoints example implementation of a simple bookstore API.

'use strict';

var express = require('express');
var bodyParser = require('body-parser');


/**
 * Creates an Express.js application which implements a Bookstore
 * API defined in `swagger.json`.
 *
 * @return {!express.Application} An initialized Express.js application.
 */
function bookstore() {
  var app = express();
  app.use(bodyParser.json());

  // Install tracing middleware.
  app.use(function(req, res, next) {
    console.log(req.method, req.originalUrl);
    console.log(req.headers);
    console.log(req.body);
    console.log();
    next();
  });

  // The bookstore example uses a simple, in-memory database
  // for illustrative purposes only.
  var bookstoreDatabase = {
    shelves: {},
    id: 0
  };

  function createShelf(theme) {
    var id = ++bookstoreDatabase.id;
    var shelf = {
      name: 'shelves/' + id,
      theme: theme,
      books: {}
    };
    bookstoreDatabase.shelves[shelf.name] = shelf;
    return shelf;
  }

  function getShelf(name) {
    return bookstoreDatabase.shelves[name];
  }

  function deleteShelf(name) {
    var shelf = bookstoreDatabase.shelves[name];
    if (shelf === undefined) {
      return undefined;
    }
    delete bookstoreDatabase.shelves[name];
    return shelf;
  }

  function createBook(shelfName, author, title) {
    var shelf = getShelf(shelfName);
    if (shelf === undefined) {
      return undefined;
    }
    var id = ++bookstoreDatabase.id;
    var book = {
      name: shelf.name + '/books/' + id,
      author: author,
      title: title
    };
    shelf.books[book.name] = book;
    return book;
  }

  function getBook(shelfName, bookName) {
    var shelf = getShelf(shelfName);
    if (shelf === undefined) {
      return undefined;
    }
    return shelf.books[bookName];
  }

  function deleteBook(shelfName, bookName) {
    var shelf = getShelf(shelfName);
    if (shelf === undefined) {
      return undefined;
    }
    var book = shelf.books[bookName];
    if (book === undefined) {
      return undefined;
    }
    delete shelf.books[bookName];
    return book;
  }

  function error(res, status, message) {
    res.status(status).json({
      error: status,
      message: message
    });
  }

  app.get('/', function(req, res) {
    res.status(200).send(process.env.VERSION);
  });

  app.get('/shelves', function(req, res) {
    var shelves = bookstoreDatabase.shelves;
    var result = [];
    for (var name in shelves) {
      var shelf = shelves[name];
      result.push({
        name: shelf.name,
        theme: shelf.theme
      });
    }
    res.status(200).json({
      shelves: result
    });
  });

  app.post('/shelves', function(req, res) {
    var shelfRequest = req.body;
    if (shelfRequest === undefined) {
      return error(res, 400, 'Missing request body.');
    }
    if (shelfRequest.theme === undefined) {
      return error(res, 400, 'Shelf resource is missing required \'theme\'.');
    }

    var shelf = createShelf(shelfRequest.theme);
    res.status(200).json({
      name: shelf.name,
      theme: shelf.theme
    });
  });

  app.get('/shelves/:shelf', function(req, res) {
    var shelf = getShelf('shelves/' + req.params.shelf);
    if (shelf === undefined) {
      return error(res, 404, 'Cannot find shelf shelves/' + req.params.shelf);
    }

    res.status(200).json({
      name: shelf.name,
      theme: shelf.theme
    });
  });

  app.delete('/shelves/:shelf', function(req, res) {
    var shelf = deleteShelf('shelves/' + req.params.shelf);
    if (shelf === undefined) {
      return error(res, 404, 'Cannot find shelf shelves/' + req.params.shelf);
    }

    res.status(204).end();
  });

  app.get('/shelves/:shelf/books', function(req, res) {
    var shelf = getShelf('shelves/' + req.params.shelf);
    if (shelf === undefined) {
      return error(res, 404, 'Cannot find shelf shelves/' + req.params.shelf);
    }

    var books = shelf.books;
    var result = [];
    for (var name in books) {
      var book = books[name];
      result.push({
        name: book.name,
        author: book.author,
        title: book.title
      });
    }

    res.status(200).json({
      books: result
    });
  });

  app.post('/shelves/:shelf/books/', function(req, res) {
    var bookRequest = req.body;
    if (bookRequest === undefined) {
      return error(res, 400, 'Missing request body.');
    }
    if (bookRequest.author === undefined) {
      return error(res, 400, 'Book resource is missing required \'author\'.');
    }
    if (bookRequest.title === undefined) {
      return error(res, 400, 'Book resource is missing required \'title\'.');
    }
    var shelf = getShelf('shelves/' + req.params.shelf);
    if (shelf === undefined) {
      return error(res, 404, 'Cannot find shelf shelves/' + req.params.shelf);
    }
    var book = createBook('shelves/' + req.params.shelf,
                          bookRequest.author,
                          bookRequest.title);
    res.status(200).json({
      name: book.name,
      author: book.author,
      title: book.title
    });
  });

  app.get('/shelves/:shelf/books/:book', function(req, res) {
    var book = getBook(
      'shelves/' + req.params.shelf,
      'shelves/' + req.params.shelf + '/books/' + req.params.book);
    if (book === undefined) {
      return error(res, 404, 'Cannot find book ' +
        'shelves/' + req.params.shelf + '/books/' + req.params.book);
    }
    res.status(200).json({
      name: book.name,
      author: book.author,
      title: book.title
    });
  });

  app.delete('/shelves/:shelf/books/:book', function(req, res) {
    var book = deleteBook(
        'shelves/' + req.params.shelf,
        'shelves/' + req.params.shelf + '/books/' + req.params.book);
    if (book === undefined) {
      return error(
          res, 404, 'Cannot find book ' +
          'shelves/' + req.params.shelf + '/books/' + req.params.book);
    }
    res.status(204).end();
  });

  // Initialize bookstoreDatabase
  (function() {
    // Initialize Bookstore
    var fiction = createShelf('Fiction');
    var fantasy = createShelf('Fantasy');

    createBook(fiction.name, 'Neal Stephenson', 'REAMDE');
    createBook(fantasy.name, 'George R.R. Martin', 'A Game of Thrones');
  })();

  return app;
}

(function() {
  var port = process.env.PORT || process.env.APP_PORT || '8080';
  var server = bookstore().listen(port, '0.0.0.0',
      function() {
        var host = server.address().address;
        var port = server.address().port;

        console.log('Bookstore listening at http://%s:%s', host, port);
      }
  );
})();
