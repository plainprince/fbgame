local frame_count = 0

function setup()
    frame_count = 0
end

function loop(dt)
    while true do
        menu.create("FBGAME DEMO", {"Audio Test", "Input Test", "Save State Test", "Quit"})
        local choice = 0
        while choice == 0 do
            choice = menu.tick()
            if choice == 0 then dt = yield() end
        end
        if choice < 0 then quit(); return end

        if choice == 1 then runAudioTest() end
        if choice == 2 then runInputTest() end
        if choice == 3 then runSaveTest() end
        if choice == 4 then quit(); return end
    end
end

function runAudioTest()
    audio.play("beep.wav")
    frame_count = 0
    while true do
        if input.keyPress("KEY_SPACE") or input.keyPress("KEY_A") then
            audio.play("beep.wav")
        end
        if input.keyPress("KEY_ESCAPE") then return end

        frame_count = frame_count + 1
        local w = render.getWidth()
        local h = render.getHeight()
        render.clear(COLOR_BLACK)
        render.text(2, 2, "AUDIO TEST", COLOR_PRIMARY)
        render.line(2, 11, w - 2, 11, COLOR_GREY)
        render.text(4, 16, "SPACE/A - play beep", COLOR_TEXT_DIM)
        render.text(4, 26, "ESC - back to menu", COLOR_TEXT_DIM)
        render.text(4, 40, "frames: " .. frame_count, COLOR_GREEN)
        render.text(2, h - 10, "ESC back", COLOR_GREY)
        dt = yield()
    end
end

function runInputTest()
    frame_count = 0
    while true do
        if input.keyPress("KEY_ESCAPE") then return end

        frame_count = frame_count + 1
        local w = render.getWidth()
        local h = render.getHeight()
        render.clear(COLOR_BLACK)
        render.text(2, 2, "INPUT TEST", COLOR_PRIMARY)
        render.text(2, 12, "ESC back", COLOR_GREY)
        render.line(2, 21, w - 2, 21, COLOR_GREY)

        local keys = { "KEY_W", "KEY_A", "KEY_S", "KEY_D", "KEY_UP", "KEY_DOWN", "KEY_SPACE", "KEY_ENTER" }
        local y = 26
        for _, k in ipairs(keys) do
            local held = input.keyHeld(k)
            local pressed = "."
            if input.keyPress(k) then pressed = "!" end
            render.text(4, y, k .. "  h:" .. tostring(held) .. " p:" .. pressed, COLOR_TEXT_DIM)
            y = y + 9
            if y > render.getHeight() - 2 then break end
        end

        dt = yield()
    end
end

function runSaveTest()
    while true do
        if input.keyPress("KEY_ESCAPE") then
            local s = save.read("state")
            local val = (s ~= "true")
            save.write("state", tostring(val))
            return
        end

        local s = save.read("state")
        local w = render.getWidth()
        local h = render.getHeight()
        render.clear(COLOR_BLACK)
        render.text(2, 2, "SAVE TEST", COLOR_PRIMARY)
        render.line(2, 11, w - 2, 11, COLOR_GREY)
        render.text(4, 16, "Saved state: " .. s, COLOR_ACCENT)
        render.text(4, 28, "ESC toggles and writes.", COLOR_TEXT_DIM)
        render.text(4, 38, "Re-enter to see change.", COLOR_TEXT_DIM)
        render.text(2, h - 10, "ESC toggle & back", COLOR_GREY)
        dt = yield()
    end
end

function shutdown()
    render.clear(COLOR_BLACK)
end
