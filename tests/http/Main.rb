# Test suite for HTTPLite.
# Copyright 2023 Splendide Imaginarius (based on Struma's docs).
# License GPLv2+.

all_tests_passed = true

System::puts "\nGET HTTP"
response = HTTPLite.get("http://httpbin.org/json")
if response[:status] == 200 #OK
    System::puts response[:body]
else
    System::puts "You got something other than an OK: #{response[:status]}"
    all_tests_passed = false
end

System::puts "\nGET HTTPS"
response = HTTPLite.get("https://httpbin.org/json")
if response[:status] == 200 #OK
    System::puts response[:body]
else
    System::puts "You got something other than an OK: #{response[:status]}"
    all_tests_passed = false
end

postdata = {
    "key1" => "value1",
    "key2" => "value2"
}

System::puts "\nPOST HTTP"
response = HTTPLite.post("http://httpbin.org/post", postdata)
if response[:status] == 200 #OK
    System::puts response[:body]
else
    System::puts "You got something other than an OK: #{response[:status]}"
    all_tests_passed = false
end

System::puts "\nPOST HTTPS"
response = HTTPLite.post("https://httpbin.org/post", postdata)
if response[:status] == 200 #OK
    System::puts response[:body]
else
    System::puts "You got something other than an OK: #{response[:status]}"
    all_tests_passed = false
end

postdata = HTTPLite::JSON.stringify(postdata)

System::puts "\nPOST body HTTP"
response = HTTPLite.post_body("http://httpbin.org/post", postdata, "application/json")
if response[:status] == 200 #OK
    System::puts response[:body]
else
    System::puts "You got something other than an OK: #{response[:status]}"
    all_tests_passed = false
end

System::puts "\nPOST body HTTPS"
response = HTTPLite.post_body("https://httpbin.org/post", postdata, "application/json")
if response[:status] == 200 #OK
    System::puts response[:body]
else
    System::puts "You got something other than an OK: #{response[:status]}"
    all_tests_passed = false
end

exit(all_tests_passed ? 0 : 1)
