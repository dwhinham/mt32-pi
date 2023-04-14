local bit = require("bit")

local function SC55Compat(status, data1, data2)
	local statusNybble = bit.band(status, 0xF0)

	-- Re-map portamento time MSB to LSB for compatibility with SC-55
	if statusNybble == 0xB0 then
		-- CC #5 - Portamento Time MSB
		if data1 == 0x05 then
			-- Convert to LSB message
			mt32pi.LCDLog(3, string.format("%02X %02X %02X -> 0x25 %02X %02X", status, data1, data2,  data1, data2))
			data1 = 0x25
			return true, status, data1, data2

		-- CC #37 - Portamento Time LSB
		elseif data1 == 0x25 then
			-- Drop the message, stop processing
			return true
		end
	end

	-- Not consumed, allow further filters
	return false
end

mt32pi.RegisterMIDIShortMessageFilter(SC55Compat)
