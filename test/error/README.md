# Simple error server for esp testing.

The server will return HTTP 400 to all requests it received.
It is primarily used to make sure that ESP tests do not accidentally
talk to a wrong version of an application.
