#--
# PDF::Writer for Ruby.
#   http://rubyforge.org/projects/ruby-pdf/
#   Copyright 2003 - 2005 Austin Ziegler.
#
#   Licensed under a MIT-style licence. See LICENCE in the main distribution
#   for full licensing information.
#
# $Id: info.rb 177 2007-11-26 16:06:58Z sandal $
#++
  # Define the document information -- metadata.
class PDF::Writer::Object::Info < PDF::Writer::Object
  Info = %w{CreationDate Creator Title Author Subject Keywords ModDate Trapped Producer}
  def initialize(parent)
    super(parent)

    @parent.instance_variable_set('@info', self)
    @creationdate = Time.now

    @creator  = File.basename($0)
    @producer = "PDF::Writer for Ruby"
    @title    = nil
    @author   = nil
    @subject  = nil
    @keywords = nil
    @moddate  = nil
    @trapped  = nil
  end

  Info.each do |i|
    attr_accessor i.downcase.intern
  end

  def to_s
    res = "\n#{@oid} 0 obj\n<<\n"
    Info.each do |i|
      v = __send__("#{i.downcase}".intern)
      next if v.nil?
      res << "/#{i} ("
      if v.kind_of?(Time)
        s = "D:%04d%02d%02d%02d%02d"
        v = v.utc
        v = s % [ v.year, v.month, v.day, v.hour, v.min ]
      end

      res << PDF::Writer.escape(v)
      res << ")\n"
    end
    res << ">>\nendobj"
  end
end
