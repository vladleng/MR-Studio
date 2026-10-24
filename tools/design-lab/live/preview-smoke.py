from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 page=b.new_page(viewport={'width':1600,'height':1000});errors=[]
 page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block')
 page.locator('[data-id="block-6"] h3').click()
 select=page.get_by_label('SVG controls/concept_pack/fader_micro_normal',exact=True)
 select.select_option('controls/concept_pack/fader_precision_normal')
 page.wait_for_function('document.querySelector("[data-id=block-6] .visual-fader").src.includes("precision")')
 page.reload();page.wait_for_selector('.block')
 assert 'precision' in page.locator('[data-id="block-6"] .visual-fader').first.get_attribute('src')
 saved=page.evaluate('localStorage.getItem("mr-design-lab-live-v1")')
 page.locator('#fullscreen-preview').click()
 page.wait_for_selector('.fullscreen-preview')
 assert page.locator('.resize').count()==0
 fader=page.locator('.preview-fader').first
 fader.fill('-20');fader.dispatch_event('input')
 assert '-20 dB' in page.locator('.mixer-strip').first.inner_text()
 page.locator('.channel-switches button').first.click()
 assert page.locator('.channel-switches button').first.get_attribute('aria-pressed')=='true'
 page.locator('[data-id="block-8"] .visual-action').first.click()
 assert page.locator('.preview-menu button').count()==3
 page.locator('.preview-menu button').first.click()
 assert page.locator('.preview-menu').count()==0
 page.locator('[data-id="block-7"] .visual-action').nth(1).click()
 assert 'PLAYING' in page.locator('.transport-readout').inner_text()
 assert saved==page.evaluate('localStorage.getItem("mr-design-lab-live-v1")')
 page.locator('#exit-preview').click()
 page.wait_for_selector('.resize')
 assert not errors, errors
 b.close()
print('PASS: fullscreen, menu, fader, mute, transport, SVG replacement/reload, layout unchanged by demo')
