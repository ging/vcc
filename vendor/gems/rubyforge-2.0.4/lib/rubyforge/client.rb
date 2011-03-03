require 'webrick/cookie'
require 'net/http'
require 'net/https'

# clean up warnings caused by web servers that send down 2 digit years
class Time
  CENTURY = Time.now.year / 100 * 100
  
  class << self
    alias :old_utc :utc

    def utc(*args)
      args[0] += CENTURY if args[0] < 100
      old_utc(*args)
    end
  end
end unless Time.respond_to? :old_utc

# clean up "using default DH parameters" warning for https
class Net::HTTP
  alias :old_use_ssl= :use_ssl=
  def use_ssl= flag
    self.old_use_ssl = flag
    @ssl_context.tmp_dh_callback = proc {} if @ssl_context
  end
end unless Net::HTTP.public_instance_methods.include? "old_use_ssl="

class RubyForge
  class Client
    attr_accessor :debug_dev, :ssl_verify_mode, :agent_class

    def initialize(proxy = nil)
      @debug_dev       = nil
      @ssl_verify_mode = OpenSSL::SSL::VERIFY_NONE
      if proxy
        begin
          proxy_uri = URI.parse(proxy)
          @agent_class = Net::HTTP::Proxy(proxy_uri.host,proxy_uri.port)
        rescue URI::InvalidURIError
        end
      end
      @agent_class ||= Net::HTTP
    end

    def post_content(uri, form = {}, headers = {}, userconfig = nil)
      uri = URI.parse(uri) unless uri.is_a?(URI)
      request = agent_class::Post.new(uri.request_uri)
      execute(request, uri, form, headers, userconfig)
    end

    def get_content(uri, query = {}, headers = {}, userconfig = nil)
      uri = URI.parse(uri) unless uri.is_a?(URI)
      request = agent_class::Get.new(uri.request_uri)
      execute(request, uri, query, headers, userconfig)
    end

    def execute(request, uri, parameters = {}, headers = {}, userconfig = nil)
      {
        'content-type' => 'application/x-www-form-urlencoded'
      }.merge(headers).each { |k,v| request[k] = v }

      http = agent_class.new( uri.host, uri.port )

      if uri.scheme == 'https' && uri.host !~ /localhost/
       http.use_ssl      = true
       http.verify_mode  = OpenSSL::SSL::VERIFY_NONE
      end
      
      request.basic_auth(userconfig["username"], userconfig["password"])

      request_data = case request['Content-Type']
                     when /boundary=(.*)$/
                       boundary_data_for($1, parameters)
                     else
                       query_string_for(parameters)
                     end
      request['Content-Length'] = request_data.length.to_s

      response = http.request(request, request_data)

      return response.body if response.class <= Net::HTTPSuccess

      if response.class <= Net::HTTPRedirection
        location = response['Location']
        unless location =~ /^http/
          location = "#{uri.scheme}://#{uri.host}#{location}"
        end
        uri = URI.parse(location)

        execute(agent_class::Get.new(uri.request_uri), uri)
      end
    end

    def boundary_data_for(boundary, parameters)
      parameters.sort_by {|k,v| k.to_s }.map { |k,v|
        parameter = "--#{boundary}\r\nContent-Disposition: form-data; name=\"" +
            WEBrick::HTTPUtils.escape_form(k.to_s) + "\""

        if v.respond_to? :path
          parameter += "; filename=\"#{File.basename(v.path)}\"\r\n"
          parameter += "Content-Transfer-Encoding: binary\r\n"
          parameter += "Content-Type: text/plain"
        end
        parameter += "\r\n\r\n"

        if v.respond_to? :path
          parameter += v.read
        else
          parameter += v.to_s
        end

        parameter
      }.join("\r\n") + "\r\n--#{boundary}--\r\n"
    end

    def query_string_for(parameters)
      parameters.sort_by {|k,v| k.to_s }.map { |k,v|
        k && [  WEBrick::HTTPUtils.escape_form(k.to_s),
                WEBrick::HTTPUtils.escape_form(v.to_s) ].join('=')
      }.compact.join('&')
    end

  end
end
