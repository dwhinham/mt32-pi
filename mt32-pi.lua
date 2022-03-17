TLCDLogType = {
	Startup = 0,
	Error = 1,
	Warning = 2,
	Notice = 3,
	Spinner = 4,
}

FirstCall = true
MessageShown = false
LastUpdate = 0
TotalTime = 0

BD = 0x24
SD = 0x26
CHH = 0x2A
OHH = 0x2E
CB = 0x38

NOTE_ON = 0x90
VELOCITY = 0x40

BPM = 90
NOTES = {
    { CHH, CB, BD },
    { CHH },
    { CHH, SD },
    { CHH, BD },
    { CHH, CB, BD },
    { CHH },
    { CHH, SD },
    { OHH }
}

NoteIndex = 1

function OnUpdate(millis)
	if FirstCall then
		LastUpdate = millis
		FirstCall = false
	end

	local deltaTime = millis - LastUpdate
	TotalTime = TotalTime + deltaTime

	-- if TotalTime >= 60 * 1000 / BPM / 2 then
	-- 	for i = 1, #NOTES[NoteIndex] do
	-- 		mt32pi.SendMIDIShortMessage(NOTE_ON | 0x09, NOTES[NoteIndex][i], VELOCITY);
	-- 	end

	-- 	NoteIndex = NoteIndex + 1
	-- 	if NoteIndex > #NOTES then
	-- 		NoteIndex = 1
	-- 	end
	-- 	TotalTime = 0
	-- end

	-- if not MessageShown and TotalTime >= 5000 then
	-- 	mt32pi.LCDLog(TLCDLogType.Notice, "Hello from Lua!")
	-- 	MessageShown = true
	-- end

	LastUpdate = millis
end

function OnMIDIShortMessage(status, data1, data2)
	local statusNybble = status & 0xF0

	-- Re-map portamento time MSB to LSB for compatiblity with SC-55
	if statusNybble == 0xB0 then
		-- CC #5 - Portamento Time MSB
		if data1 == 0x05 then
			-- Convert to LSB message
			data1 = 0x25

		-- CC #37 - Portamento Time LSB
		elseif data1 == 0x25 then
			-- Drop the message
			return
		end
	end

	return status, data1, data2
end

-- function OnMIDIShortMessage(status, data1, data2)
-- 	local statusNybble = status & 0xF0
-- 	local channelNybble = status & 0x0F

-- 	-- Note On
-- 	if statusNybble == 0x90 then
-- 		-- -- Max velocity
-- 		-- if (data2 > 0) then
-- 		-- 	data2 = 0x7F;
-- 		-- end

-- 		-- Not percussison channel
-- 		-- if channelNybble ~= 9 then
-- 		-- 	-- Transpose +1
-- 		-- 	data1 = data1 + 1;
-- 		-- end

-- 	-- Note Off
-- 	elseif statusNybble == 0x80 then
-- 		-- Not percussion channel
-- 		-- if channelNybble ~= 9 then
-- 		-- 	-- Transpose +1
-- 		-- 	data1 = data1 + 1;
-- 		-- end

-- 	-- Control Change
-- 	elseif statusNybble == 0xB0 then
-- 		-- CC# 3 - Unused
-- 		if data1 == 0x03 then
-- 			-- Switch to SoundFont
-- 			--mt32pi.SendMIDISysExMessage("\xF0\x7D\x03\x01\xF7");
-- 			BPM = data2
-- 			return;
-- 		elseif data1 == 0x1F then
-- 			-- Map to master volume control
-- 			mt32pi.SetMasterVolume(data2 / 127 * 100);

-- 			mt32pi.SendMIDIShortMessage(0x90, data2, 0x3F);
-- 			return;
-- 		end
-- 	end

-- 	-- Print MIDI message on LCD
-- 	-- mt32pi.LCDLog(TLCDLogType.Notice, string.format("0x%02X%02X%02X", status, data1, data2))

-- 	return status, data1, data2
-- end

function OnMIDISysExMessage(data)
	-- Pass through
	return data
end
