from playwright.sync_api import sync_playwright
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===6')
 page.locator('#layers button').filter(has_text='Timeline / patches').click()
 page.get_by_label('SVG rig/amp_head',exact=True).select_option('icons/pedal')
 page.wait_for_function('Array.from(document.querySelectorAll("[data-slot]")).find(n=>n.dataset.slot==="rig/amp_head").src.includes("pedal")')
 page.reload();page.wait_for_selector('.track-patch');assert 'pedal' in page.locator('[data-slot="rig/amp_head"]').first.get_attribute('src')
 saved=page.evaluate('localStorage.getItem("mr-design-lab-live-v1")')
 page.locator('#fullscreen-preview').click();page.wait_for_selector('.fullscreen-preview');assert page.locator('.resize').count()==0
 page.locator('[data-action="view.mixer"]').click();mixer=page.locator('.runtime-mixer');fader=mixer.locator('.preview-fader').first;fader.fill('-20');fader.dispatch_event('input');fader.dispatch_event('change')
 assert '-20 dB' in mixer.locator('.mixer-strip').first.inner_text()
 mixer.locator('.channel-switches button').first.click();assert mixer.locator('.channel-switches button').first.get_attribute('aria-pressed')=='true'
 mixer.get_by_role('button',name='Закрыть',exact=True).click()
 page.locator('[data-action="show.open"]').click();assert page.locator('.preview-menu button').count()==3;page.locator('.preview-menu button').first.click()
 page.locator('[data-action="transport.play"]').click();assert 'PLAYING' in page.locator('.transport-readout').inner_text()
 assert saved==page.evaluate('localStorage.getItem("mr-design-lab-live-v1")')
 page.locator('#exit-preview').click();page.wait_for_selector('.resize');assert not errors,errors;b.close()
print('PASS: fullscreen, icon mixer, fader/mute, toolbar/transport, SVG replacement/reload, independent layout')
