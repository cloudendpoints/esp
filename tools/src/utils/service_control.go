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
package utils

import (
	"bytes"
	"fmt"
	"log"
	"math"
	"sort"

	"github.com/golang/protobuf/proto"
	"github.com/golang/protobuf/ptypes/struct"
	"google/api/servicecontrol/v1"
	"google/logging/type"
)

type ExpectedCheck struct {
	Version       string
	ServiceName   string
	ConsumerID    string
	OperationName string
	CallerIp      string
}

type ExpectedReport struct {
	Version           string
	ServiceName       string
	ApiName           string
	ApiVersion        string
	ApiMethod         string
	ApiKey            string
	ProducerProjectID string
	URL               string
	Location          string
	HttpMethod        string
	LogMessage        string
	RequestSize       int64
	ResponseSize      int64
	ResponseCode      int
	Referer           string
	StatusCode        string
	ErrorCause        string
	ErrorType         string
	Protocol          string
	Platform          string
	JwtAuth           string
}

type distOptions struct {
	Buckets int32
	Growth  float64
	Scale   float64
}

var (
	timeDistOptions = distOptions{8, 10.0, 1e-6}
	sizeDistOptions = distOptions{8, 10.0, 1}
	randomMatrics   = map[string]bool{
		"serviceruntime.googleapis.com/api/consumer/total_latencies":            true,
		"serviceruntime.googleapis.com/api/producer/total_latencies":            true,
		"serviceruntime.googleapis.com/api/consumer/backend_latencies":          true,
		"serviceruntime.googleapis.com/api/producer/backend_latencies":          true,
		"serviceruntime.googleapis.com/api/consumer/request_overhead_latencies": true,
		"serviceruntime.googleapis.com/api/producer/request_overhead_latencies": true,
	}
	randomLogEntries = []string{
		"timestamp",
		"request_latency_in_ms",
	}
	fakeLatency = 1000
)

func CreateCheck(er *ExpectedCheck) servicecontrol.CheckRequest {
	erPb := servicecontrol.CheckRequest{
		ServiceName: er.ServiceName,
		Operation: &servicecontrol.Operation{
			OperationName: er.OperationName,
			ConsumerId:    er.ConsumerID,
			Labels: map[string]string{
				"servicecontrol.googleapis.com/user_agent":    "ESP",
				"servicecontrol.googleapis.com/service_agent": "ESP/" + er.Version,
			},
		},
	}
	if er.CallerIp != "" {
		erPb.Operation.Labels["servicecontrol.googleapis.com/caller_ip"] =
			er.CallerIp
	}
	return erPb
}

func responseCodes(code int) (response, status, class string) {
	return fmt.Sprintf("%d", code),
		fmt.Sprintf("%d", HttpResponseCodeToStatusCode(code)),
		fmt.Sprintf("%dxx", code/100)
}

func createReportLabels(er *ExpectedReport) map[string]string {
	response, status, class := responseCodes(er.ResponseCode)
	labels := map[string]string{
		"servicecontrol.googleapis.com/service_agent": "ESP/" + er.Version,
		"servicecontrol.googleapis.com/user_agent":    "ESP",
		"serviceruntime.googleapis.com/api_method":    er.ApiMethod,
		"cloud.googleapis.com/location":               er.Location,
		"/response_code":                              response,
		"/status_code":                                status,
		"/response_code_class":                        class,
		"/protocol":                                   "http",
	}
	if er.StatusCode != "" {
		labels["/status_code"] = er.StatusCode
	}
	if er.ErrorType != "" {
		labels["/error_type"] = er.ErrorType
	}
	if er.Protocol != "" {
		labels["/protocol"] = er.Protocol
	}
	if er.ApiVersion != "" {
		labels["serviceruntime.googleapis.com/api_version"] = er.ApiVersion
	}
	if er.Platform != "" {
		labels["servicecontrol.googleapis.com/platform"] = er.Platform
	} else {
		labels["servicecontrol.googleapis.com/platform"] = "unknown"
	}
	if er.ApiKey != "" {
		labels["/credential_id"] = "apikey:" + er.ApiKey
	} else if er.JwtAuth != "" {
		labels["/credential_id"] = "jwtauth:" + er.JwtAuth
	}
	return labels
}

func makeStringValue(v string) *structpb.Value {
	return &structpb.Value{&structpb.Value_StringValue{v}}
}

func makeNumberValue(v int64) *structpb.Value {
	return &structpb.Value{&structpb.Value_NumberValue{float64(v)}}
}

func createLogEntry(er *ExpectedReport) *servicecontrol.LogEntry {
	pl := make(map[string]*structpb.Value)

	pl["api_method"] = makeStringValue(er.ApiMethod)
	pl["http_response_code"] = makeNumberValue(int64(er.ResponseCode))

	if er.ApiVersion != "" {
		pl["api_version"] = makeStringValue(er.ApiVersion)
	}
	if er.ProducerProjectID != "" {
		pl["producer_project_id"] = makeStringValue(er.ProducerProjectID)
	}
	if er.ApiKey != "" {
		pl["api_key"] = makeStringValue(er.ApiKey)
	}
	if er.Referer != "" {
		pl["referer"] = makeStringValue(er.Referer)
	}
	if er.Location != "" {
		pl["location"] = makeStringValue(er.Location)
	}
	if er.RequestSize != 0 {
		pl["request_size"] = makeNumberValue(er.RequestSize)
	}
	if er.ResponseSize != 0 {
		pl["response_size"] = makeNumberValue(er.ResponseSize)
	}
	if er.LogMessage != "" {
		pl["log_message"] = makeStringValue(er.LogMessage)
	}
	if er.URL != "" {
		pl["url"] = makeStringValue(er.URL)
	}
	if er.HttpMethod != "" {
		pl["http_method"] = makeStringValue(er.HttpMethod)
	}
	if er.ErrorCause != "" {
		pl["error_cause"] = makeStringValue(er.ErrorCause)
	}

	severity := _type.LogSeverity_INFO
	if er.ResponseCode >= 400 {
		severity = _type.LogSeverity_ERROR
	}

	return &servicecontrol.LogEntry{
		Name:     "endpoints_log",
		Severity: severity,
		Payload: &servicecontrol.LogEntry_StructPayload{
			&structpb.Struct{
				Fields: pl,
			},
		},
	}
}

func createInt64MetricSet(name string, value int64) *servicecontrol.MetricValueSet {
	return &servicecontrol.MetricValueSet{
		MetricName: name,
		MetricValues: []*servicecontrol.MetricValue{
			&servicecontrol.MetricValue{
				Value: &servicecontrol.MetricValue_Int64Value{value},
			},
		},
	}
}

func createDistMetricSet(options *distOptions, name string, value int64) *servicecontrol.MetricValueSet {
	buckets := make([]int64, options.Buckets+2, options.Buckets+2)
	fValue := float64(value)
	idx := 0
	if fValue >= options.Scale {
		idx = 1 + int(math.Log(fValue/options.Scale)/math.Log(options.Growth))
		if idx >= len(buckets) {
			idx = len(buckets) - 1
		}
	}
	buckets[idx] = 1
	distValue := servicecontrol.Distribution{
		Count:        1,
		BucketCounts: buckets,
		BucketOption: &servicecontrol.Distribution_ExponentialBuckets_{
			&servicecontrol.Distribution_ExponentialBuckets{
				NumFiniteBuckets: options.Buckets,
				GrowthFactor:     options.Growth,
				Scale:            options.Scale,
			},
		},
	}

	if value != 0 {
		distValue.Mean = fValue
		distValue.Minimum = fValue
		distValue.Maximum = fValue
	}
	return &servicecontrol.MetricValueSet{
		MetricName: name,
		MetricValues: []*servicecontrol.MetricValue{
			&servicecontrol.MetricValue{
				Value: &servicecontrol.MetricValue_DistributionValue{&distValue},
			},
		},
	}
}

func updateDistMetricSet(m *servicecontrol.MetricValueSet, fValue float64) {
	for _, v := range m.MetricValues {
		d := v.GetDistributionValue()
		o := d.GetExponentialBuckets()

		d.Mean = fValue
		d.Minimum = fValue
		d.Maximum = fValue

		buckets := d.BucketCounts
		idx := 0
		if fValue >= o.Scale {
			idx = 1 + int(math.Log(fValue/o.Scale)/math.Log(o.GrowthFactor))
			if idx >= len(buckets) {
				idx = len(buckets) - 1
			}
		}
		for i, _ := range buckets {
			buckets[i] = 0
		}
		buckets[idx] = 1
	}
}

type metricSetSorter []*servicecontrol.MetricValueSet

func (a metricSetSorter) Len() int           { return len(a) }
func (a metricSetSorter) Swap(i, j int)      { a[i], a[j] = a[j], a[i] }
func (a metricSetSorter) Less(i, j int) bool { return a[i].MetricName < a[j].MetricName }

func CreateReport(er *ExpectedReport) servicecontrol.ReportRequest {
	op := servicecontrol.Operation{
		OperationName: er.ApiMethod,
	}

	if er.ApiKey != "" {
		op.ConsumerId = "api_key:" + er.ApiKey
	}
	op.Labels = createReportLabels(er)

	op.LogEntries = []*servicecontrol.LogEntry{
		createLogEntry(er),
	}

	ms := []*servicecontrol.MetricValueSet{
		createInt64MetricSet("serviceruntime.googleapis.com/api/consumer/request_count", 1),
		createInt64MetricSet("serviceruntime.googleapis.com/api/producer/request_count", 1),

		createDistMetricSet(&sizeDistOptions,
			"serviceruntime.googleapis.com/api/consumer/request_sizes", er.RequestSize),
		createDistMetricSet(&sizeDistOptions,
			"serviceruntime.googleapis.com/api/producer/request_sizes", er.RequestSize),
	}
	for name, _ := range randomMatrics {
		ms = append(ms, createDistMetricSet(&timeDistOptions, name, int64(fakeLatency)))
	}
	if er.ResponseSize != 0 {
		ms = append(ms,
			createDistMetricSet(&sizeDistOptions,
				"serviceruntime.googleapis.com/api/consumer/response_sizes", er.ResponseSize),
			createDistMetricSet(&sizeDistOptions,
				"serviceruntime.googleapis.com/api/producer/response_sizes", er.ResponseSize))
	}
	if er.ErrorType != "" {
		ms = append(ms,
			createInt64MetricSet("serviceruntime.googleapis.com/api/consumer/error_count", 1),
			createInt64MetricSet("serviceruntime.googleapis.com/api/producer/error_count", 1))
	}
	sort.Sort(metricSetSorter(ms))
	op.MetricValueSets = ms

	erPb := servicecontrol.ReportRequest{
		ServiceName: er.ServiceName,
		Operations:  []*servicecontrol.Operation{&op},
	}
	return erPb
}

func stripRandomFields(op *servicecontrol.Operation) {
	// Clear some fields
	op.OperationId = ""
	op.StartTime = nil
	op.EndTime = nil

	for _, m := range op.MetricValueSets {
		if _, found := randomMatrics[m.MetricName]; found {
			updateDistMetricSet(m, float64(fakeLatency))
		}
	}
	sort.Sort(metricSetSorter(op.MetricValueSets))

	for _, l := range op.LogEntries {
		l.Timestamp = nil
		for _, s := range randomLogEntries {
			delete(l.GetStructPayload().Fields, s)
		}
	}
}

func compareProto(t, e proto.Message) bool {
	if proto.Equal(t, e) {
		return true
	}
	var ts bytes.Buffer
	if err := proto.MarshalText(&ts, t); err == nil {
		fmt.Println("=== Got:\n", ts.String())
	}
	var es bytes.Buffer
	if err := proto.MarshalText(&es, e); err == nil {
		fmt.Println("=== Expected:\n", es.String())
	}
	return false
}

func VerifyCheck(body []byte, er *ExpectedCheck) bool {
	cr := servicecontrol.CheckRequest{}
	err := proto.Unmarshal(body, &cr)
	if err != nil {
		log.Println("failed to parse body into CheckRequest.")
		return false
	}
	stripRandomFields(cr.Operation)

	erPb := CreateCheck(er)
	return compareProto(&cr, &erPb)
}

func VerifyReport(body []byte, er *ExpectedReport) bool {
	cr := servicecontrol.ReportRequest{}
	err := proto.Unmarshal(body, &cr)
	if err != nil {
		log.Println("failed to parse body into ReportRequest.")
		return false
	}
	for _, op := range cr.Operations {
		stripRandomFields(op)
	}

	erPb := CreateReport(er)
	return compareProto(&cr, &erPb)
}
