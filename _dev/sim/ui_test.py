import asyncio, json, os, subprocess, sys
os.chdir(os.path.dirname(os.path.abspath(__file__))); os.makedirs("shots", exist_ok=True)
from playwright.async_api import async_playwright
URL = "http://potahto.local:8080/"
def dev(cmd, **kw):
    out = subprocess.run(["curl", "-s", "-X", "POST", "http://127.0.0.1:8080/api?pin=123456", "-d", json.dumps({"cmd": cmd, **kw})], capture_output=True, text=True).stdout
    return json.loads(out)
fails = []
def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond: fails.append(msg)
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(args=["--host-resolver-rules=MAP potahto.local 127.0.0.1"])
        for scheme in ["light", "dark"]:
            ctx = await b.new_context(viewport={"width": 1200, "height": 900}, color_scheme=scheme)
            pg = await ctx.new_page()
            errs = []
            pg.on("console", lambda m: errs.append(m.text) if m.type == "error" else None)
            pg.on("pageerror", lambda e: errs.append(str(e)))
            await pg.goto(URL)
            check(await pg.is_visible("#connect"), f"[{scheme}] connect screen shown")
            check(not await pg.is_visible("#usbCard"), f"[{scheme}] USB card hidden when served by device")
            if scheme == "light": await pg.screenshot(path="shots/0_connect.png")
            await pg.fill("#pin", "000000"); await pg.click("#btnWifi"); await pg.wait_for_timeout(500)
            check("Wrong PIN" in await pg.inner_text("#connErr"), f"[{scheme}] wrong PIN rejected")
            await pg.fill("#pin", "123456"); await pg.click("#btnWifi")
            await pg.wait_for_selector("#tabs:not(.hidden)", timeout=5000)
            await pg.wait_for_timeout(800)
            check("73%" in await pg.inner_text("#pages"), f"[{scheme}] dashboard shows battery")
            check(not await pg.is_visible("#bleCard"), f"[{scheme}] Bluetooth card hidden when served by device")
            dtxt = await pg.inner_text("#pages")
            check("keys & bluetooth" in dtxt.lower() and "USB" in dtxt and "(internal)" in dtxt, f"[{scheme}] dashboard shows route + time source")
            if scheme == "light":
                await pg.click("text=Screen off"); await pg.wait_for_timeout(500)
                check(dev("status")["screen"] is False, "dashboard Screen off turns the screen off")
                await pg.click("text=Wake"); await pg.wait_for_timeout(500)
                check(dev("status")["screen"] is True, "dashboard Wake turns it back on")
            await pg.screenshot(path=f"shots/1_dash_{scheme}.png", full_page=True)
            bad = []
            for tid in await pg.locator("nav button").evaluate_all("bs => bs.map(b => b.dataset.id)"):
                await pg.click(f"nav button[data-id={tid}]"); await pg.wait_for_timeout(700)
                t = await pg.inner_text("#pages")
                if "Error:" in t: bad.append(tid + ": " + t[t.index("Error:"):][:80])
            check(not bad, f"[{scheme}] every page renders without errors {bad}")
            await pg.click("nav button[data-id=dash]"); await pg.wait_for_timeout(500)
            # settings
            await pg.click("nav button[data-id=settings]"); await pg.wait_for_timeout(600)
            cards = [c.lower() for c in await pg.locator("#pages .card h3").all_inner_texts()]
            check("air mouse" in cards and "sound" in cards and "wifi" not in cards, f"[{scheme}] settings groups rendered {cards}")
            await pg.screenshot(path=f"shots/2_settings_{scheme}.png", full_page=True)
            if scheme == "light":
                # toggle a switch: General > Boot sound
                before = dev("get", keys=["sSound"])["values"]["sSound"]
                row = pg.locator(".field", has_text="Boot sound")
                await row.locator(".sw span").click(); await pg.wait_for_timeout(700)
                check(dev("get", keys=["sSound"])["values"]["sSound"] == (not before), "switch writes to device")
                # segmented control: Air Mouse > Screen -> 1080p
                await pg.locator(".field", has_text="Screen").locator("button", has_text="1080p").click(); await pg.wait_for_timeout(700)
                check(dev("get", keys=["mScreen"])["values"]["mScreen"] == 1, "segmented control writes enum index")
                # slider: Sensitivity -> set via keyboard
                sl = pg.locator(".field", has_text="Sensitivity").locator("input[type=range]")
                await sl.focus(); await pg.keyboard.press("End"); await pg.wait_for_timeout(700)
                check(dev("get", keys=["mSens"])["values"]["mSens"] == 20, "slider writes value")
                # advanced toggle reveals axis settings
                await pg.locator(".sectionhead .sw span").click(); await pg.wait_for_timeout(600)
                check(await pg.get_by_text("Horizontal axis", exact=True).count() == 1, "advanced reveals axis settings")
                await pg.screenshot(path="shots/3_settings_adv.png", full_page=True)
                # text field: device name
                inp = pg.locator(".field", has_text="Device name").locator("input")
                await inp.fill("Spud"); await inp.press("Tab"); await pg.wait_for_timeout(700)
                check(dev("get", keys=["devName"])["values"]["devName"] == "Spud", "text field writes")
                # wifi page
                await pg.click("nav button[data-id=wifi]"); await pg.wait_for_timeout(600)
                await pg.click("text=Scan"); await pg.wait_for_timeout(3500)
                items = await pg.locator(".li .grow").all_inner_texts()
                check(items == ["HomeNet", "CampusNet", "Neighbour 5G", "CafeFree"], f"scan list deduped + sorted {items}")
                check("username" in await pg.locator(".li", has_text="CampusNet").inner_text(), "enterprise network marked 'username'")
                await pg.screenshot(path="shots/4_wifi.png", full_page=True)
                await pg.locator(".li", has_text="CafeFree").locator("button").click()
                await pg.keyboard.type("latte123")
                await pg.locator(".li", has_text="CampusNet").locator("button").click()
                await pg.keyboard.type("student01")
                await pg.locator("input[type=password]").nth(1).fill("uni-pass")
                await pg.click("text=Save networks"); await pg.wait_for_timeout(800)
                v = dev("get", keys=["wSsid0", "wSsid1", "wEap1", "wUser1"])["values"]
                check(v.get("wSsid0") == "CafeFree", f"network saved to slot {v}")
                check(v.get("wSsid1") == "CampusNet" and v.get("wEap1") == 1 and v.get("wUser1") == "student01", f"enterprise network saved with username {v}")
                sch = {f["k"]: f for f in dev("schema")["fields"]}
                check(sch["wPass0"].get("set") is True and "v" not in sch["wPass0"], "password stored, never sent back over HTTP")
                # games
                await pg.click("nav button[data-id=games]"); await pg.wait_for_timeout(600)
                check("42" in await pg.inner_text("#pages"), "hiscores shown")
                # firmware page renders upload controls over wifi
                await pg.click("nav button[data-id=fw]"); await pg.wait_for_timeout(400)
                check(await pg.locator("input[type=file]").count() == 1, "firmware upload control shown over WiFi")
                await pg.screenshot(path="shots/5_fw.png", full_page=True)
                # macros page
                await pg.click("nav button[data-id=macros]"); await pg.wait_for_timeout(900)
                chips = await pg.locator(".chip:not(.lib)").all_inner_texts()
                check(chips == ["Windows", "Media", "VS Code", "Chrome", "PowerPoint", "Teams", "Zoom"], f"macro profiles listed {chips}")
                tabs = await pg.locator("nav button").all_inner_texts()
                check(tabs[:4] == ["Dashboard", "Settings", "WiFi", "Macros"], f"tab order {tabs}")
                await pg.locator(".chip:not(.lib)", has_text="VS Code").click(); await pg.wait_for_timeout(300)
                heads = await pg.locator("#pages .card h3").all_inner_texts()
                check(sum("folder" in x.lower() for x in heads) == 4, f"folders layout shows 4 folder pads {heads[:6]}")
                await pg.locator(".chip:not(.lib)", has_text="Windows").click(); await pg.wait_for_timeout(300)
                up = pg.locator(".pad .k").nth(0)
                await up.locator("input.m").fill("{ctrl+zz}"); await pg.wait_for_timeout(900)
                check("unknown key 'zz'" in await up.locator(".e").inner_text(), "live macro validation shows device error")
                await up.locator("input.m").fill("{ctrl+shift+n}"); await pg.wait_for_timeout(900)
                check(await up.locator(".e").inner_text() == "", "error clears when fixed")
                await up.locator("input").first.fill("New win")
                await pg.locator("#pages .card input[type=text]").first.fill("Win")
                await pg.click("text=Library"); await pg.wait_for_timeout(200)
                lib = await pg.locator(".chip.lib").all_inner_texts()
                check("✓ VS Code" in lib and "+ Word" in lib, f"library marks profiles already added {lib[:6]}")
                await pg.locator(".chip.lib", has_text="Word").click(); await pg.wait_for_timeout(300)
                await pg.screenshot(path="shots/7_macros.png", full_page=True)
                await pg.click("text=Save changes"); await pg.wait_for_timeout(800)
                d = dev("macro.get")["data"]["profiles"]
                check(len(d) == 8 and d[0]["name"] == "Win" and d[0]["keys"][0] == ["New win", "{ctrl+shift+n}"] and d[7]["name"] == "Word" and d[7]["app"], "macros + library profile saved to device")
                await pg.locator(".chip:not(.lib)", has_text="Media").click(); await pg.wait_for_timeout(300)
                await pg.locator(".pad .k").nth(2).locator("input.m").fill("{nope}")
                await pg.click("text=Save changes"); await pg.wait_for_timeout(800)
                check("unknown key" in await pg.inner_text("#toast"), "save rejected with reason")
                check(dev("macro.get")["data"]["profiles"][1]["keys"][4][1] == "{media playpause}", "device kept old value on rejected save")
                # smart page
                await pg.click("nav button[data-id=smart]"); await pg.wait_for_timeout(900)
                await pg.click("text=Add webhook"); await pg.wait_for_timeout(200)
                hook = pg.locator(".hook").last
                await hook.locator("input[placeholder='https://…']").fill("https://example.com/hook?e={event}")
                await pg.locator(".card", has_text="Smart Button keys").locator("select").first.select_option("0")
                await pg.click("text=Add rule"); await pg.wait_for_timeout(200)
                mq = pg.locator(".card", has_text="Home Assistant / MQTT")
                await mq.locator(".field", has_text="Enabled").locator(".sw span").click()
                await mq.locator("input[placeholder='192.168.1.10']").fill("10.0.0.5")
                await mq.locator("input[type=password]").fill("pw1")
                await pg.screenshot(path="shots/8_smart.png", full_page=True)
                await pg.click("text=Save changes"); await pg.wait_for_timeout(900)
                c = dev("iot.get")["cfg"]
                check(c["mqtt"]["on"] and c["mqtt"]["host"] == "10.0.0.5" and c["mqtt"]["passSet"] and "pass" not in c["mqtt"], "MQTT saved, password write-only")
                check(c["hooks"][0]["url"] == "https://example.com/hook?e={event}" and c["buttons"][0]["hook"] == 0 and len(c["rules"]) == 1, f"webhook/keys/rules saved")
                await pg.locator(".hook").last.locator("text=Test").click(); await pg.wait_for_timeout(3200)
                check("HTTP 200" in await pg.inner_text("#hookres"), "webhook test result shown")
                # desk page
                await pg.click("nav button[data-id=desk]"); await pg.wait_for_timeout(1000)
                card = pg.locator(".card", has_text="Alarms")
                await card.locator("input[type=time]").first.fill("06:45"); await card.locator("input[type=time]").first.dispatch_event("change"); await pg.wait_for_timeout(500)
                await card.locator(".days").first.locator("button").nth(5).click(); await pg.wait_for_timeout(500)
                v = dev("get", keys=["al0On", "al0H", "al0M", "al0D"])["values"]
                check(v == {"al0On": True, "al0H": 6, "al0M": 45, "al0D": 31 | 32}, f"alarm saved {v}")
                await pg.click("text=Start >> nth=0"); await pg.wait_for_timeout(600)
                await pg.locator(".card", has_text="Notifications").locator("input[placeholder=Message]").fill("Hello from the web")
                await pg.locator(".card", has_text="Notifications").get_by_role("button", name="Send", exact=True).click(); await pg.wait_for_timeout(600)
                check("Hello from the web" in await pg.inner_text("#pages"), "notification sent + listed")
                gcard = pg.locator(".card", has_text="Desk Guard")
                await gcard.get_by_role("button", name="Arm").first.click(); await pg.wait_for_timeout(600)
                check(dev("watch")["guard"] is True, "Desk Guard armed from the web app")
                check("Disarm" in await gcard.inner_text(), "guard card shows Disarm")
                mi = gcard.locator(".field", has_text="When someone comes").locator("input").first
                await mi.fill("{alt+tab}{run chrome}"); await mi.press("Tab"); await pg.wait_for_timeout(900)
                check(dev("get", keys=["dgNearM"])["values"]["dgNearM"] == "{alt+tab}{run chrome}", "guard 'someone comes' macro saved")
                await pg.screenshot(path="shots/9_desk.png", full_page=True)
                # music page
                await pg.click("nav button[data-id=music]"); await pg.wait_for_timeout(1000)
                mt = await pg.inner_text("#pages")
                check("Spud Knight" in mt and "Night Light" in mt, "music: built-in songs listed")
                await pg.click("text=New song"); await pg.wait_for_timeout(200)
                cv = pg.locator("canvas")
                for i, (x, y) in enumerate([(32, 200), (46, 180), (60, 160), (74, 140)]):
                    await cv.click(position={"x": x, "y": y}); await pg.wait_for_timeout(80)
                check("4/512 notes" in (await pg.inner_text("#pages")).lower(), "composer: clicks add notes")
                await pg.click("text=Save to Potahto"); await pg.wait_for_timeout(900)
                ls = dev("song.list")["songs"]
                check(len(ls) == 1 and ls[0]["name"].startswith("My song"), f"composer: song saved on device {ls}")
                await pg.screenshot(path="shots/11_music.png", full_page=True)
                # toolkit page
                await pg.click("nav button[data-id=toolkit]"); await pg.wait_for_timeout(3500)
                txt = await pg.inner_text("#pages")
                check("Buds Pro" in txt and "iBeacon" in txt, "toolkit: BLE list rendered")
                check("Least crowded" in txt and "CafeFree" in txt, "toolkit: WiFi survey rendered")
                await pg.click("text=Discover"); await pg.wait_for_timeout(1500)
                check("homeassistant" in await pg.inner_text("#pages"), "toolkit: LAN hosts listed")
                await pg.click("text=Run checks"); await pg.wait_for_timeout(3500)
                check("HTTP 204" in await pg.inner_text("#pages"), "toolkit: net doctor finished")
                await pg.click("text=Add PC"); await pg.wait_for_timeout(200)
                await pg.locator("input[placeholder='AA:BB:CC:DD:EE:FF']").last.fill("bad")
                await pg.click("button.pri:has-text('Save')"); await pg.wait_for_timeout(600)
                check(len(dev("wol.get")["devices"]) == 1, "toolkit: bad MAC rejected")
                await pg.locator("input[placeholder='AA:BB:CC:DD:EE:FF']").last.fill("11:22:33:44:55:66")
                await pg.click("button.pri:has-text('Save')"); await pg.wait_for_timeout(600)
                check(len(dev("wol.get")["devices"]) == 2, "toolkit: WoL PC saved")
                await pg.screenshot(path="shots/10_toolkit.png", full_page=True)
                await pg.click("nav button[data-id=settings]"); await pg.wait_for_timeout(700)
                sett = [c.lower() for c in await pg.locator("#pages .card h3").all_inner_texts()]
                check("alarms" not in sett and "wifi" not in sett and "pomodoro" in sett and "desk guard" in sett, f"custom fields hidden from Settings {sett}")
                # mobile layout
                await pg.set_viewport_size({"width": 390, "height": 844})
                await pg.click("nav button[data-id=settings]"); await pg.wait_for_timeout(600)
                await pg.screenshot(path="shots/6_mobile.png", full_page=False)
                wide = []
                for tid in await pg.locator("nav button").evaluate_all("bs => bs.map(b => b.dataset.id)"):
                    await pg.click(f"nav button[data-id={tid}]"); await pg.wait_for_timeout(700)
                    sw = await pg.evaluate("document.documentElement.scrollWidth")
                    if sw > 390: wide.append(f"{tid}={sw}")
                    if tid == "music": await pg.screenshot(path="shots/12_music_phone.png", full_page=True)
                check(not wide, f"no horizontal scroll on phone width {wide}")
            check(not errs, f"[{scheme}] no console errors {errs[:3]}")
            await ctx.close()
        await b.close()
asyncio.run(main())
print("\n%d failures" % len(fails)); sys.exit(1 if fails else 0)
