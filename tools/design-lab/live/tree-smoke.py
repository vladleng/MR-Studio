from playwright.sync_api import sync_playwright
import json
fixture={'schema':'mr-live-demo-show','version':1,'folders':[{'id':'a','name':'Pack A','parent':None},{'id':'b','name':'Pack B','parent':None},{'id':'c','name':'Nested','parent':'a'}],'items':[{'id':x,'name':name,'folder':folder,'tempo':92,'meter':'4/4','key':'C','duration':240,'notes':''} for x,name,folder in [('one','Song one','a'),('two','Song two','b'),('three','Song three','c')]],'setlists':[{'id':'set','name':'Show','entries':[{'id':'e1','itemId':'one'},{'id':'e2','itemId':'two'}]}],'activeSetlist':'set','currentItem':'one'}
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 page=b.new_page(viewport={'width':1920,'height':1200});errors=[]
 page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block')
 page.evaluate('(data)=>localStorage.setItem("mr-live-demo-show-v1",data)',json.dumps(fixture));page.reload();page.wait_for_selector('.block')
 page.locator('#load-user-layout').click();page.wait_for_function('document.querySelectorAll(".block").length===8');page.locator('#preview').check()
 root=page.locator('.show-browser');root.get_by_role('button',name='Browser',exact=True).click()
 assert root.get_by_role('button',name='+ Папка',exact=True).count()==0
 assert root.get_by_role('button',name='Экспорт demo show',exact=True).count()==0
 root.locator('.show-rows').dispatch_event('contextmenu',{'clientX':200,'clientY':300})
 page.get_by_role('menu').get_by_role('menuitem',name='+ Папка',exact=True).click()
 page.get_by_role('dialog').get_by_label('Название',exact=True).fill('Created by menu')
 page.get_by_role('dialog').get_by_role('button',name='Сохранить',exact=True).click()
 a=root.get_by_role('button',name='Папка Pack A',exact=True);a.click()
 assert a.get_attribute('aria-expanded')=='true'
 assert root.get_by_role('button',name='Папка Pack B',exact=True).is_visible()
 root.get_by_role('button',name='Папка Nested',exact=True).click()
 assert root.locator('[data-item=three]').count()==1
 root.locator('[data-item=one]').click(button='right')
 menu=page.get_by_role('menu');assert menu.is_visible()
 menu.get_by_role('menuitem',name='Свойства',exact=True).click()
 assert page.get_by_role('dialog').get_by_label('Название',exact=True).input_value()=='Song one'
 page.get_by_role('button',name='Отмена',exact=True).click()
 root.locator('[data-item=one]').drag_to(root.get_by_role('button',name='Папка Pack B',exact=True))
 stored=json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 assert next(i for i in stored['items'] if i['id']=='one')['folder']=='b'
 assert root.locator('[data-item=one]').count()==0
 root.get_by_role('button',name='Папка Pack B',exact=True).click()
 assert root.locator('[data-item=one]').count()==1
 assert root.locator('[data-item=three]').count()==1
 root.get_by_role('button',name='Папка Pack B',exact=True).click(button='right')
 page.get_by_role('menu').get_by_role('menuitem',name='Переименовать папку',exact=True).click()
 page.get_by_role('dialog').get_by_label('Название',exact=True).fill('Pack B renamed');page.get_by_role('dialog').get_by_role('button',name='Сохранить',exact=True).click()
 root.get_by_role('button',name='Setlist',exact=True).click();root.locator('[data-item=one]').click(button='right')
 page.get_by_role('menu').get_by_role('menuitem',name='Убрать из setlist',exact=True).click()
 assert root.locator('.song-row').count()==1
 root.locator('.show-rows').dispatch_event('contextmenu',{'clientX':200,'clientY':300});page.get_by_role('menu').get_by_role('menuitem',name='Undo show',exact=True).click();assert root.locator('.song-row').count()==2
 root.get_by_role('button',name='Browser',exact=True).click()
 page.locator('#fullscreen-preview').click();page.wait_for_selector('.fullscreen-preview')
 root.get_by_role('button',name='Папка Pack A',exact=True).click(button='right')
 assert page.get_by_role('menu').is_visible();page.keyboard.press('Escape')
 page.locator('#exit-preview').click() if page.locator('#exit-preview').is_visible() else None
 assert not errors,errors
 b.close()
print('PASS: in-place nested tree, cross-folder drag persistence, item/folder/setlist context actions, Undo, fullscreen menu')
