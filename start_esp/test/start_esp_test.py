#!/usr/bin/python -u
#
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

"""Tests to check that the ESP configuration files are as expected

Before starting ESP, start_esp executable generates the ESP configuration files based on its input arguments.
To test that the generated ESP configuration files are as expected, run:

   bazel test //start_esp/test:start_esp_test
"""

import unittest
import os
import os.path

class TestStartEsp(unittest.TestCase):
    nginx_conf_template = "./start_esp/test/nginx-conf-template"
    server_conf_template = "./start_esp/test/server-conf-template"
    generated_nginx_config_file = "./start_esp/test/nginx.conf"
    generated_server_config_file = "./start_esp/test/generated_server_configuration.json"
    empty_flag_config_generator = "./start_esp/test/start_esp_binary --generate_config_file_only --server_config_generation_path ./start_esp/test/generated_server_configuration.json"
    basic_config_generator = "./start_esp/test/start_esp_binary --generate_config_file_only --pid_file ./start_esp/test/pid_file --service_account_key key --config_dir ./start_esp/test --template ./start_esp/test/nginx-conf-template --server_config_template ./start_esp/test/server-conf-template --service_json_path ./start_esp/test/testdata/test_service_config_1.json --server_config_generation_path ./start_esp/test/generated_server_configuration.json"
    backend_routing_config_generator = "./start_esp/test/start_esp_binary --enable_backend_routing --generate_config_file_only --pid_file ./start_esp/test/pid_file --service_account_key key --config_dir ./start_esp/test --template ./start_esp/test/nginx-conf-template --server_config_template ./start_esp/test/server-conf-template --service_json_path ./start_esp/test/testdata/test_service_config_1.json --server_config_generation_path ./start_esp/test/generated_server_configuration.json"

    @staticmethod
    def file_equal(path1, path2):
        f1 = open(path1, "r")
        c1 = f1.readlines()
        f1.close()
        f2 = open(path2, "r")
        c2 = f2.readlines()
        f2.close()
        # Remove blank lines
        c1 = filter(lambda x: x.strip(), c1)
        c2 = filter(lambda x: x.strip(), c2)
        # Strip blank chars
        c1 = map(str.strip, c1)
        c2 = map(str.strip, c2)
        return c1 == c2

    # Run the test and check that the generated file is as expected
    def run_test_with_expectation(self, expected_config_file, generated_config_file, config_generator):
        self.assertTrue(os.path.isfile(expected_config_file), "the expected config file does not exist")
        self.assertTrue(os.path.isfile(self.nginx_conf_template), "the template config file does not exist")
        os.system(config_generator)
        self.assertTrue(os.path.isfile(generated_config_file), "the config file is not generated")
        is_equal = TestStartEsp.file_equal(generated_config_file, expected_config_file)
        if not is_equal :
            print("The generated config does not match the expected config")
            print("The generated config is:")
            f = open(generated_config_file, "r")
            print(f.read())
            f.close()
            print("The expected config is:")
            f = open(expected_config_file, "r")
            print(f.read())
            f.close()
        self.assertTrue(is_equal, "the generated config file is as not as expected")

    ########## The tests for generating the nginx configuration file start from here ##########

    def test_basic_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_basic_nginx.conf"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, self.basic_config_generator)

    def test_experimental_enable_multiple_api_configs_disabled_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_experimental_enable_multiple_api_configs_disabled_nginx.conf"
        config_generator = self.basic_config_generator + ' --service "service1|service2"'
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_backend_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_backend_nginx.conf"
        config_generator = self.basic_config_generator + " --backend https://1.2.3.4:12345"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_status_port_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_status_port_nginx.conf"
        config_generator = self.basic_config_generator + " --status_port 56789"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_metadata_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_metadata_nginx.conf"
        config_generator = self.basic_config_generator + " --metadata http://1.2.3.4"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_dns_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_dns_nginx.conf"
        config_generator = self.basic_config_generator + " --dns 1.2.3.4"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_access_log_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_access_log_nginx.conf"
        config_generator = self.basic_config_generator + " --access_log test_access_log"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_healthz_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_healthz_nginx.conf"
        config_generator = self.basic_config_generator + " --healthz test_healthz"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_underscores_in_headers_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_underscores_in_headers_nginx.conf"
        config_generator = self.basic_config_generator + " --underscores_in_headers"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_enable_websocket_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_enable_websocket_nginx.conf"
        config_generator = self.basic_config_generator + " --enable_websocket"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_enable_debug_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_enable_debug_nginx.conf"
        config_generator = self.basic_config_generator + " --enable_debug"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_client_max_body_size_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_client_max_body_size_nginx.conf"
        config_generator = self.basic_config_generator + " --client_max_body_size 10m"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_client_body_buffer_size_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_client_body_buffer_size_nginx.conf"
        config_generator = self.basic_config_generator + " --client_body_buffer_size 100k"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_allow_invalid_headers_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_allow_invalid_headers_nginx.conf"
        config_generator = self.basic_config_generator + " --allow_invalid_headers"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_worker_processes_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_worker_processes_nginx.conf"
        config_generator = self.basic_config_generator + " --worker_processes 1234"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_ssl_protocols_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_ssl_protocols_nginx.conf"
        config_generator = self.basic_config_generator + " --ssl_protocols=TLSv1.1 --ssl_protocols=TLSv1.2"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_ssl_port_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_ssl_port_nginx.conf"
        config_generator = self.basic_config_generator + " --backend https://1.2.3.4.5:12345 --ssl_port 6789"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_basic_cors_preset_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cors_preset_basic_nginx.conf"
        config_generator = self.basic_config_generator + " --cors_preset basic"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_cors_allow_origin_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cors_allow_origin_nginx.conf"
        config_generator = self.basic_config_generator + " --cors_preset basic --cors_allow_origin *.google.com"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_cors_allow_methods_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cors_allow_methods_nginx.conf"
        config_generator = self.basic_config_generator + " --cors_preset basic --cors_allow_methods 'GET, POST'"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_cors_allow_headers_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cors_allow_headers_nginx.conf"
        config_generator = self.basic_config_generator + " --cors_preset basic --cors_allow_headers 'DNT,User-Agent'"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_cors_allow_credentials_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cors_allow_credentials_nginx.conf"
        config_generator = self.basic_config_generator + " --cors_preset basic --cors_allow_credentials"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_cors_expose_headers_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cors_expose_headers_nginx.conf"
        config_generator = self.basic_config_generator + " --cors_preset basic --cors_expose_headers 'Content-Length'"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_cors_allow_origin_regex_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cors_allow_origin_regex_nginx.conf"
        config_generator = self.basic_config_generator + " --cors_preset cors_with_regex --cors_allow_origin_regex test_cors_regex"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_backend_host_header_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_backend_host_header_nginx.conf"
        config_generator = self.basic_config_generator + " --experimental_proxy_backend_host_header your.backend.host"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    def test_enable_strict_transport_security_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_enable_strict_transport_security_nginx.conf"
        config_generator = self.basic_config_generator + " --enable_strict_transport_security"
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    ########## The tests for generating the server configuration file start from here ##########

    def test_service_control_url_override_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_service_control_url_override_server.json"
        config_generator = self.basic_config_generator + " --service_control_url_override test_url_override"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_rollout_strategy_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_rollout_strategy_server.json"
        config_generator = self.basic_config_generator + " --rollout_strategy managed"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_client_ip_header_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_client_ip_header_server.json"
        config_generator = self.basic_config_generator + " --client_ip_header test_ip_header"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_client_ip_position_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_client_ip_position_server.json"
        config_generator = self.basic_config_generator + " --client_ip_header test_ip_header --client_ip_position 1"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_management_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_management_server.json"
        config_generator = self.basic_config_generator + " --management https://test-management.googleapis.com"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_transcoding_always_print_primitive_fields_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_transcoding_always_print_primitive_fields_server.json"
        config_generator = self.basic_config_generator + " --transcoding_always_print_primitive_fields"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_rewrite_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_rewrite_server.json"
        config_generator = self.basic_config_generator + " --rewrite test_rewrite_rule_1 --rewrite test_rewrite_rule_2"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_disable_cloud_trace_auto_sampling_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_disable_cloud_trace_auto_sampling_server.json"
        config_generator = self.basic_config_generator + " --disable_cloud_trace_auto_sampling"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_cloud_trace_url_override_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_cloud_trace_url_override_server.json"
        config_generator = self.basic_config_generator + " --cloud_trace_url_override test_cloud_trace_url_override"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_service_control_report_http_headers_arg_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_service_control_report_http_headers.json"
        config_generator = self.basic_config_generator + " --log_request_headers foo,bar --log_response_headers foo"
        self.run_test_with_expectation(expected_config_file, self.generated_server_config_file, config_generator)

    def test_backend_routing_output_is_as_expected(self):
        expected_config_file = "./start_esp/test/testdata/expected_backend_routing_nginx.conf"
        config_generator = self.backend_routing_config_generator
        self.run_test_with_expectation(expected_config_file, self.generated_nginx_config_file, config_generator)

    ########## The tests for validating it should generate failure on conflict flags ##########

    def test_enable_backend_routing_conflicts_with_string_flag(self):
        config_generator = self.empty_flag_config_generator + " --enable_backend_routing --pid_file fake_value"
        return_code = os.system(config_generator)
        self.assertEqual(return_code >> 8, 3)

    def test_enable_backend_routing_conflicts_with_boolean_flag(self):
        config_generator = self.empty_flag_config_generator + " --enable_backend_routing --non_gcp"
        return_code = os.system(config_generator)
        self.assertEqual(return_code >> 8, 3)

    def test_enable_backend_routing_conflicts_with_single_dash_flag(self):
        config_generator = self.empty_flag_config_generator + " --enable_backend_routing -z fake_value"
        return_code = os.system(config_generator)
        self.assertEqual(return_code >> 8, 3)

if __name__ == '__main__':
    unittest.main()
