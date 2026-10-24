from playwright.sync_api import sync_playwright
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===6')
 page.locator('#layers button').filter(has_text='Transport').click()
 for width in [400,700]:
  page.locator('[name=w]').fill(str(width));page.locator('[name=w]').press('Tab')
  row=page.locator('.visual-control-transport .visual-actions').bounding_box();time=page.locator('.transport-readout').bounding_box();assert row['x']+row['width']<=time['x'];assert abs(row['y']+row['height']/2-time['y']-time['height']/2)<2
 page.locator('#preview').check();page.locator('[data-action="view.mixer"]').click();mixer=page.locator('.runtime-mixer')
 assert mixer.locator('.strip-inserts').count()==4;assert mixer.locator('.strip-sends').count()==4;assert mixer.locator('.pan-slider').count()==4;assert mixer.locator('[data-toggle]').count()==16
 volume=mixer.get_by_role('slider',name='Громкость Vocal',exact=True);volume.fill('-18');volume.dispatch_event('input');volume.dispatch_event('change');assert '-18 dB' in mixer.locator('.mixer-strip').last.inner_text()
 pan=mixer.get_by_role('slider',name='Панорама Guitar',exact=True);pan.fill('35');pan.dispatch_event('input');pan.dispatch_event('change');assert pan.locator('..').inner_text()=='35R'
 mixer.locator('[data-menu="send"]').first.click();assert mixer.locator('.preview-menu').is_visible();assert not errors,errors;b.close()
print('PASS: transport single line and icon mixer hierarchy/volume/pan/send menu')
