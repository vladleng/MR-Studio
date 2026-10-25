from playwright.sync_api import sync_playwright
import json
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block');page.locator('#load-user-layout').click();page.locator('#preview').check();root=page.locator('.block .show-browser');page.locator('.live-item-container').first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 active=doc()['currentItem'];other=root.locator('.song-row').nth(1);other.dblclick();name=other.get_by_role('textbox',name='Название item');name.fill('Zulu rename');name.press('Enter');page.wait_for_timeout(650);assert doc()['currentItem']==active;assert root.locator('.item-drag-handle').count()==0
 other=root.locator('.song-row').filter(has_text='Zulu rename');other.drag_to(root.locator('.song-row').first,target_position={'x':15,'y':2});assert doc()['currentItem']==active;page.wait_for_timeout(400);other=root.locator('.song-row').filter(has_text='Zulu rename');iid=other.get_attribute('data-item');other.click();page.wait_for_timeout(550);assert doc()['currentItem']==iid
 root.get_by_role('button',name='Browser',exact=True).click();library=page.locator('.runtime-items .show-browser');names=library.locator('.song-item-name').all_inner_texts();assert ['Autumn Leaves','Moon River','Zulu rename']==[n.split('. ',1)[1] for n in names]
 page.locator('.runtime-items').get_by_role('button',name='Закрыть',exact=True).click()
 fixture=doc();fixture['folders']=[{'id':'folder-z','name':'10 folder','parent':None},{'id':'folder-a','name':'2 folder','parent':None}]
 for index,item in enumerate(fixture['items']):item['folder']='folder-a' if index<2 else 'folder-z'
 bar=page.get_by_role('navigation',name='Меню MR Live');bar.get_by_role('button',name='Files',exact=True).click()
 with page.expect_file_chooser() as chosen:page.get_by_role('menuitem',name='Открыть…',exact=True).click()
 chosen.value.set_files({'name':'folders.json','mimeType':'application/json','buffer':json.dumps(fixture).encode()})
 root.get_by_role('button',name='Browser',exact=True).click();library=page.locator('.runtime-items .show-browser');assert library.locator('.folder-row').all_inner_texts()==['▸ 2 folder','▸ 10 folder'];library.get_by_role('button',name='Папка 2 folder',exact=True).click();names=library.locator('[data-folder="folder-a"].tree-group .song-item-name').all_inner_texts();assert [n.split('. ',1)[1] for n in names]==['Moon River','Zulu rename'];page.locator('.runtime-items').get_by_role('button',name='Закрыть',exact=True).click()
 first=page.locator('.live-item-container').first;first.get_by_role('button',name='Mute: Keys',exact=True).click();mute=first.get_by_role('button',name='Mute: Keys',exact=True);assert mute.evaluate('(n)=>getComputedStyle(n).backgroundColor')=='rgb(230, 143, 136)'
 first.get_by_role('button',name='Solo: Keys',exact=True).click();solo=first.get_by_role('button',name='Solo: Keys',exact=True);assert solo.evaluate('(n)=>getComputedStyle(n).backgroundColor')=='rgb(226, 192, 112)'
 page.locator('[data-action="view.mixer"]').click();strip=page.locator('.runtime-mixer .mixer-strip[data-channel]').first;assert strip.locator('[data-toggle=M]').evaluate('(n)=>getComputedStyle(n).backgroundColor')=='rgb(230, 143, 136)';assert strip.locator('[data-toggle=S]').evaluate('(n)=>getComputedStyle(n).backgroundColor')=='rgb(226, 192, 112)'
 assert not errors,errors;b.close()
print('PASS: no drag handles, inactive double-click rename, passive body drag, delayed single focus, alphabetical browser, red M/yellow S')
