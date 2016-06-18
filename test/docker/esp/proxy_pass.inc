# configuration settings for proxy_pass to main app
proxy_pass http://app_server;
proxy_redirect off;
proxy_http_version 1.1;
proxy_set_header Connection "";
proxy_set_header Host $host;
proxy_set_header X-Real-IP $remote_addr;
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
proxy_set_header X-Forwarded-Host $server_name;
proxy_set_header X-Google-Real-IP $remote_addr;
# 86400 seconds (24 hours) is the maximum a server is allowed.
proxy_send_timeout 86400s;
proxy_read_timeout 86400s;
