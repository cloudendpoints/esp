# Multiple Services #

For simplicity, we recommend that an ESP instance proxies only one API,
defined as an Endpoints service.

However, it is possible to enable an experimental feature that proxies
multiple Endpoints services as of version 1.24.0.

To do so, enable the --enable_experimental_multiple_api_config and provide
the service names in a pipe-separated string to the
`--service` command line argument of `start_esp`. You will also have to
supply your own `nginx.conf` template file using the `--template` argument,
since the default template assumes a single `server_config` file at the
location `/etc/nginx/server_config.pb.txt`.

For example, if you have two services you want to proxy,
`svc1.example.com` and `svc2.example.com`, and a custom `nginx.conf` template
in the file `/etc/nginx/my-nginx.conf.template`, you can run ESP with this
command:

    /usr/sbin/start_esp --enable_experimental_multiple_api_config --template=/etc/nginx/my-nginx.conf.template \
        --service="svc1.example.com|svc2.example.com" \
        --rollout_strategy=managed

This command will generate two `server_config` files: 
`svc1.example.com_server_config.txt` and `svc2.example.com_server_config.txt`.
You must use the full path to these files in the `server_config` elements in
the `endpoints` blocks in your `nginx.conf` template.

You can use either `start_esp/nginx-auto.conf.template` or
`docker/custom/nginx.conf.template` as a starting point for your own
`nginx.conf` template.

The `start_esp` command will also download the latest service configuration
for each service from the Service Management API and place them in separate
files in the `/etc/nginx/endpoints` directory. These files will have
auto-generated names. The generated `server_config` files mentioned above
will have references to the downloaded service configuration files.
