from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click();bar=page.get_by_role('navigation',name='Меню MR Live',exact=True)
 assert page.locator('.visual-control-toolbar [data-action="show.open"]').count()==0
 assert page.locator('.visual-control-toolbar [data-action="show.save"]').count()==0
 bar.get_by_role('button',name='Files',exact=True).click()
 with page.expect_download() as download:page.get_by_role('menuitem',name='Сохранить',exact=True).click()
 saved=json.load(open(download.value.path()));assert saved['schema']=='mr-live-demo-show'
 bar.get_by_role('button',name='Files',exact=True).click()
 with page.expect_file_chooser() as choice:page.get_by_role('menuitem',name='Открыть…',exact=True).click()
 choice.value.set_files({'name':'show.json','mimeType':'application/json','buffer':json.dumps(saved).encode()})
 bar.get_by_role('button',name='Options',exact=True).click();page.get_by_role('menuitem',name='View record tracks',exact=True).click();assert page.locator('.record-track').count()==6
 bar.get_by_role('button',name='Transport',exact=True).click();page.get_by_role('menuitem',name='Play',exact=True).click();assert 'PLAYING' in page.locator('.transport-readout').inner_text()
 bar.get_by_role('button',name='Transport',exact=True).click();page.get_by_role('menuitem',name='Stop',exact=True).click()
 page.locator('[data-action="view.mixer"]').click();mixer=page.locator('.runtime-mixer');assert mixer.locator('.runtime-drawer-header').count()==0;assert mixer.get_by_role('button',name='Закрыть',exact=True).count()==0
 tools=mixer.locator('.mixer-tools');assert tools.get_by_role('button',name='+ Канал',exact=True).count()==1;assert tools.get_by_role('button',name='Record · общая запись',exact=True).count()==1;assert tools.get_by_role('button',name='Stop',exact=True).count()==1
 page.locator('[data-action="view.mixer"]').click();assert mixer.count()==0
 assert first.get_by_role('button',name='+ Часть',exact=True).count()==0;assert first.get_by_role('button',name='Добавить аккорд',exact=True).count()==0;assert 'Sections' in first.locator('.item-parts-label').first.inner_text()
 sections=first.locator('.item-parts-row').bounding_box();chords=first.locator('.item-chords-row').bounding_box();assert abs(chords['y']-sections['y']-sections['height'])<2,(sections,chords)
 part=first.locator('.song-part').first;part.click(button='right');page.get_by_role('menuitem',name='Создать Chorus',exact=True).click();assert first.locator('.song-part').count()==5
 # Remove all sections to exercise double-click creation in empty space.
 while first.locator('.song-part').count():
  first.locator('.song-part').last.click(button='right');page.get_by_role('menuitem',name='Удалить часть',exact=True).click()
 first.locator('.section-lane').dblclick(position={'x':40,'y':10});assert first.locator('.song-part').count()==1;assert 'Verse' in first.locator('.song-part').inner_text()
 first.locator('.chord-lane').dblclick(position={'x':40,'y':10});assert first.locator('.chord-event').count()==1
 assert not errors,errors;b.close()
print('PASS: Files export/import, Options/Transport menus, compact mixer tools/toggle close, Sections templates/doubleclick, adjacent chord lane without add buttons')
