#!/usr/bin/env ruby

# Die if something goes wrong
def die(msg); puts(msg); exit!(1); end

# Login credentials
login = `git config --get github.user`.chomp
token = `git config --get github.token`.chomp

# The file we want to upload
require 'pathname'
file = Pathname.new(ARGV[0])
class Pathname; def type; `file -Ib #{to_s}`.chomp; end; end

# Repository to which to upload the file
repo = ARGV[1] || `git config --get remote.origin.url`.match(/git@github.com:(.+?)\.git/)[1]

# Establish a HTTPS connection to github
require 'net/https'
uri = URI.parse("https://github.com/#{repo}/downloads")
http = Net::HTTP.new(uri.host, uri.port)
http.use_ssl = true

# Request a signature and that stuff so that we can upload the file to S3
req = Net::HTTP::Post.new(uri.path)
req.set_form_data({
  'login' => login, 'token' => token, 'file_size' => file.size.to_s,
  'content_type' => file.type, 'file_name' => file.basename.to_s,
})

# Check if something went wrong
res = http.request(req)
die("File already exists.") if res.class == Net::HTTPClientError
die("Github doens't want us to upload the file: #{res}") unless res.class == Net::HTTPOK

# Parse the body, it's json
require 'json'
info = JSON.parse(res.body)

# Open a connection to S3
uri = URI.parse('http://github.s3.amazonaws.com/')
http = Net::HTTP.new(uri.host, uri.port)

# Yep, ruby net/http doesn't support multipart. Write our own multipart generator.

def urlencode(str)
  str.gsub(/[^a-zA-Z0-9_\.\-]/n) {|s| sprintf('%%%02x', s[0].to_i) }
end

def build_multipart_content(params)
  parts, boundary = [], "#{rand(1000000)}-we-are-all-doomed-#{rand(1000000)}"

  params.each do |name, value|
    data = []
    if value.is_a?(Pathname) then
      data << "Content-Disposition: form-data; name=\"#{urlencode(name.to_s)}\"; filename=\"#{value.basename}\""
      data << "Content-Type: #{value.type}"
      data << "Content-Length: #{value.size}"
      data << "Content-Transfer-Encoding: binary"
      data << ""
      data << value.read
    else
      data << "Content-Disposition: form-data; name=\"#{urlencode(name.to_s)}\""
      data << ""
      data << value
    end

    parts << data.join("\r\n") + "\r\n"
  end

  [ "--#{boundary}\r\n" + parts.join("--#{boundary}\r\n") + "--#{boundary}--", {
    "Content-Type" => "multipart/form-data; boundary=#{boundary}"
  }]
end

# The order of the params is important, the file needs to go as last!
res = http.post(uri.path, *build_multipart_content({
  'key' => info['path'], 'Filename' => file.basename.to_s, 'policy' => info['policy'],
  'AWSAccessKeyId' => info['accesskeyid'], 'signature' => info['signature'],
  'acl' => info['acl'], 'success_action_status' => '201', 'Content-Type' => file.type,
  'file' => file
}))

die("S3 is mean to us.") unless res.class == Net::HTTPCreated

# Print the URL to the file to stdout.
puts "http://github.s3.amazonaws.com/#{info['path']}"
