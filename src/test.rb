#!/usr/bin/ruby

# test.rb creates a roughly N step random macro for ne to
# run on a text or binary file. Only an ne built with NE_TEST
# defined will be able to run this macro; otherwise it will
# stop on any command that can't succeed, like moving past
# the end of the document. The macro makes arbitrary changes,
# saves the document, undoes all the changes, saves again,
# and finally redoes the changes and saves again, then exits.
# You're supposed to compare the various saved files to see
# if the undone file is byte-for-byte identical to the
# original, and if the redone file is identical to the .test
# version. If they are not, then ne is broken.
# 
# By far the easiest way to use the program is to source
# test.rc. It uses test.rb to create a non-binary macro
# which it runs on ne.texinfo, a binary macro which it
# runs on ./ne itself, reports the results and more or
# less cleans up after itself.
# 
# Do not let a "NE_TEST" version of the ne binary out into
# the wild; it does not do the Right Thing for production
# use.

if ARGV.length == 0 
	puts "Args: N FILE [BINARY]"
	exit
end

if ARGV.length > 2 then puts("BINARY"); end
block = false
clip = false
a = IO.readlines(ARGV[1])
utf8 = true
ops = 0

puts("OPEN \"" + ARGV[1] + "\"")
puts("TURBO 1000000000")

ARGV[0].to_i.times do |i|

	r = (rand * 100).to_i

	if r < 10 then # Move around
		case rand(25)
		when 0
			puts("GOTOCOLUMN " + (rand(80)+1).to_s)
		when 1
			puts("GOTOLINE " + (rand(a.length)+1).to_s)
		when 2
			puts("LINEUP " + (rand(100)+1).to_s)
		when 3
			puts("LINEDOWN " + (rand(100)+1).to_s)
		when 4
			puts("MOVEBOS")
		when 5
			puts("MOVEEOF")
		when 6
			puts("MOVEEOL")
		when 7
			puts("MOVEEOW")
		when 8
			puts("MOVEINCDOWN")
		when 9
			puts("MOVEINCUP")
		when 10
			puts("MOVELEFT " + (rand(80)).to_s)
		when 11
			puts("MOVERIGHT " + (rand(80)).to_s)
		when 12
			puts("MOVESOF")
		when 13
			puts("MOVESOL")
		when 14
			puts("MOVETOS")
		when 15
			puts("NEXTPAGE")
		when 16
			puts("NEXTWORD " + (rand(20)).to_s)
		when 17
			puts("PAGEDOWN")
		when 18
			puts("PAGEUP")
		when 19
			puts("PREVPAGE")
		when 20
			puts("PREVWORD " + (rand(20)).to_s)
		when 21
			puts("SETBOOKMARK")
		when 22
			puts("GOTOBOOKMARK")
		when 23
			puts("GOTOBOOKMARK -")
		when 24
			puts("UNSETBOOKMARK")
		end
	elsif r < 20 then # Changing flags
		case rand(7)
		when 0
			puts("FREEFORM")
		when 1
			puts("INSERT")
		when 2
			puts("AUTOINDENT")
		when 3
			puts("RIGHTMARGIN " + (rand(80) + 1).to_s)
		when 4
			puts("WORDWRAP")
		when 5
			puts("TABS")
		when 6
			puts("TABSIZE " + (rand(20) + 1).to_s)
		end

	elsif r < 30 # Deleting text
		ops += 1
		case rand(4)
		when 0
			puts("BACKSPACE " + (rand(20)).to_s)
		when 1
			puts("DELETECHAR " + (rand(20)).to_s)
		when 2
			puts("DELETEEOL")
		when 3
			puts("DELETELINE " + (rand(10)).to_s)
		end

	elsif r < 40 
		if block
			case rand(4)
	when 0
	puts("COPY")
	clip = true
	when 1
	puts("CUT")
	clip = true
	when 2
	puts("ERASE")
	when 3
	puts("THROUGH sort")
			end
			block = false
		else
			case rand(4)
	when 0
	if clip then 
		puts("PASTE")
		clip = false
	else 
		puts("MARK")
		block = true
	end
	when 1
	if clip then 
		puts("PASTEVERT")
		clip = false
	else 
		puts("MARKVERT")
		block = true
	end
			end
		end
	

	elsif r < 50 # Editing
		case rand(11)
		when 0
			puts("CAPITALIZE " + (rand(10)).to_s)
		when 1
			puts("CENTER " + (rand(10)).to_s)
		when 2
			begin
			s = a[rand(a.length)].chomp
			end while s.length == 0
			start = rand(s.length/2);
			puts("FIND \"" + s[start..start+rand(s.length/2)] + "\"")
			begin
			s = a[rand(a.length)].chomp
			end while s.length == 0
			start = rand(s.length/2);
			puts("REPLACEALL \"" + s[start..start+rand(s.length/2)] + "\"")
		when 3
			case rand(3)
			when 0
				puts("FINDREGEXP [A-Z][a-z]+")
			when 1
				puts("FINDREGEXP [a-z]+");
			when 2
				puts("FINDREGEXP [0-9]+");
			end

			begin
				s = a[rand(a.length)].chomp
			end while s.length == 0
			start = rand(s.length/2);
			puts("REPLACEALL \"" + s[start..start+rand(s.length/2)] + "\"")
		when 4
			puts("PARAGRAPH " + (rand(20)).to_s)
		when 5
			t = rand(Math::sqrt(ops).to_i + 1)
			puts("UNDO " + t.to_s)
			puts("REDO " + t.to_s)
		when 6
			puts("TOLOWER " + rand(10).to_s) 
		when 7
			puts("TOUPPER " + rand(10).to_s)
		when 8
			puts("UNDELLINE " + (rand(10)).to_s)
		when 9
			puts("AUTOCOMPLETE")
		when 10
#			puts("SHIFT " + (rand(2)==0?"<":">") + rand(20) + (rand(2)==0?"t":"s"));
		end

	else # Generate text
		ops += 1
		case rand(4)
			when 0
			s = a[rand(a.length)].chomp
			puts("INSERTCHAR " + (rand(126) + 1).to_s)
			when 1
			puts("INSERTLINE")
			when 2
			begin
			s = a[rand(a.length)].chomp
			end while s.length == 0
			puts("INSERTSTRING \"" + s + "\"")
		end
	end

end

puts("SAVEAS \"" + ARGV[1] + ".test\"")
puts("UNDO 10000000");
puts("SAVEAS \"" + ARGV[1] + ".undone\"")
puts("REDO 10000000")
puts("SAVEAS \"" + ARGV[1] + ".redone\"")
puts("EXIT");
