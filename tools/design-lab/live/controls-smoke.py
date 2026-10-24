from playwright.sync_api import sync_playwright
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 page=b.new_page(viewport={'width':1600,'height':1000});errors=[]
 page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block')
 transport=page.locator('[data-id="block-7"]')
 for width in [400,700]:
  transport.locator('h3').click();page.locator('[name=w]').fill(str(width));page.locator('[name=w]').press('Tab')
  row=transport.locator('.visual-actions').bounding_box();time=transport.locator('.transport-readout').bounding_box()
  assert row['x']+row['width']<=time['x']
  assert abs(row['y']+row['height']/2-time['y']-time['height']/2)<2
 mixer=page.locator('[data-id="block-6"]');mixer.locator('h3').click()
 page.locator('[name=h]').fill('400');page.locator('[name=h]').press('Tab')
 page.locator('[data-command=front]').click()
 assert mixer.locator('.strip-inserts').count()==4
 assert mixer.locator('.strip-sends').count()==4
 assert mixer.locator('.pan-slider').count()==4
 assert mixer.locator('[data-toggle]').count()==16
 page.locator('#preview').check()
 volume=page.locator('[data-id="block-5"]').get_by_role('slider',name='Громкость Vocal',exact=True)
 volume.fill('-18');volume.dispatch_event('input')
 assert volume.locator('..').inner_text()=='-18 dB'
 pan=page.locator('[data-id="block-5"]').get_by_role('slider',name='Панорама Guitar',exact=True)
 pan.fill('35');pan.dispatch_event('input')
 assert pan.locator('..').inner_text()=='35R'
 mixer.locator('[data-menu="send"]').first.click()
 assert mixer.locator('.preview-menu').is_visible()
 assert not errors,errors
 page.locator('#preview').uncheck()
 page.screenshot(path='/tmp/mr-live-controls.png')
 b.close()
print('PASS: transport single line, Studio mixer hierarchy, patch volume/pan, send menu')
