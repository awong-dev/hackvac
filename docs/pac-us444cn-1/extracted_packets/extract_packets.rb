#!/usr/bin/ruby
#
# This is a simple packet extractor from a serial analyzer dump out of Logic.
# Make sure to sort the csv file by time BEFORE giving it to this script.

# Yay. Mixin from https://mentalized.net/journal/2011/04/14/ruby-how-to-check-if-a-string-is-numeric/.
class String
  def numeric?
    Float(self) != nil rescue false
  end
end

class Packet
  attr_accessor :start_byte_ts, :end_byte_ts, :data, :pin
  def initialize()
    reset(nil)
  end

  def reset(pin)
    @start_byte_ts = 0
    @end_byte_ts = 0
    @data = []
    @pin = pin
  end

  def invalid_checksum()
    checksum = data[0..-2].reduce(0xfc){ |s, b| s - b } & 0xff
    checksum == data[-1] ? '' : ' !!'
  end

  def to_s()
    "[#{start_byte_ts.round(1)}, #{end_byte_ts.round(1)}] s:#{data.size} #{pin} : #{data.map{|d| d.to_s(16)}.join(',')}#{invalid_checksum}"
  end
end

def print_packet(outfile, current_packet, last_ts)
  if not current_packet.data.empty?
    gap = (current_packet.start_byte_ts - last_ts)
    outfile.puts "  ~ gap #{gap.round(1)}ms ~"
    outfile.puts current_packet
  end
end

puts ARGV[1]
File.open(ARGV[1], "w") do |outfile|
  last_ts = 0
  current_packet = Packet.new

  File.foreach(ARGV[0]) do |line|
    cols = line.split(',')
    if cols.size == 3 and cols[0].numeric?
      ts = cols[0].to_f * 1000
      if not cols[1].eql?(current_packet.pin)
        print_packet(outfile, current_packet, last_ts)
        last_ts = current_packet.end_byte_ts
        current_packet.reset(cols[1])
        current_packet.start_byte_ts = ts
      end

      # Parse the hex out of the last field.
      hex = /.*\(0x(..)\).*/.match(cols[2])[1]
      current_packet.end_byte_ts = ts
      current_packet.data << hex.to_i(16)
    end
  end

  print_packet(outfile, current_packet, last_ts)
end
