local cwc = cwc

cwc.connect_signal("client::map", function(client)
    -- unmanaged client is a popup/tooltip client in xwayland so lets skip it.
    if client.unmanaged then return end

    -- center the client from the screen workarea if its floating or in floating layout.
    if client.floating then client:center() end

    -- don't pass focus when the focused client is fullscreen but allow if the parent is the focused
    -- one. Useful when gaming where an app may restart itself and steal focus.
    local focused = cwc.client.focused()
    if focused and focused.fullscreen and client.parent ~= focused then
        client:lower()
        return
    end

    client:raise()
    client:focus()
end)

cwc.connect_signal("client::unmap", function(client)
    -- exit when the unmapped client is not the focused client.
    if client ~= cwc.client.focused() then return end
    -- and for unmanaged client
    if client.unmanaged then return end

    -- if the client container has more than one client then we focus just below the unmapped
    -- client
    local cont_stack = client.container.client_stack
    if #cont_stack > 1 then
        cont_stack[2]:focus()
    else
        -- get the focus stack (first item is the newest) and we shift focus to the second newest
        -- since first one is about to be unmapped from the screen.
        local latest_focus_after = client.screen:get_focus_stack(true)[2]
        if latest_focus_after then latest_focus_after:focus() end
    end
end)

cwc.connect_signal("client::focus", function(client)
    -- by default when a client got focus it's not raised so we raise it.
    -- should've been hardcoded to the compositor since that's the intuitive behavior
    -- but it's nice to have option I guess.
    client:raise()
end)

cwc.connect_signal("container::insert", function(cont, client)
    -- reset mark after first insertion in case forgot to toggle off mark
    cwc.container.reset_mark()

    -- focus to the newly inserted client
    client:focus()
end)

cwc.connect_signal("screen::mouse_enter", function(s)
    s:focus()
end)
