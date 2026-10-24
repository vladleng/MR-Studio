from playwright.sync_api import sync_playwright
import json,math
with sync_playwright() as p:
 b=p.chromium.launch(executable_path='/usr/bin/chromium',args=['--no-sandbox']);page=b.new_page(viewport={'width':1920,'height':1200});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto('http://127.0.0.1:8001/tools/design-lab/live/');page.wait_for_selector('.block')
 page.locator('#load-user-layout').click();page.get_by_role('button',name='Mixer / meters',exact=True).click();assert page.locator('.visual-mixer-channels').count()==1
 page.locator('#load-user-layout').click();page.locator('#preview').check()
 first=page.locator('.live-item-container').first;first.get_by_role('button',name='Открыть item',exact=True).click()
 def doc():return json.loads(page.evaluate('localStorage.getItem("mr-live-demo-show-v1")'))
 setlist=page.locator('.block .show-browser');setlist.get_by_role('button',name='Browser',exact=True).click();library=page.locator('.runtime-items');assert library.is_visible()
 setbox=setlist.locator('..').bounding_box();libbox=library.bounding_box();assert abs(libbox['x']-setbox['x']-setbox['width'])<2,(setbox,libbox)
 original=doc();items=len(original['items']);count=len(original['setlists'][0]['entries']);iid=original['items'][1]['id']
 # Actual browser-to-setlist copy; original item/folder stays in the library.
 library.locator(f'.song-row[data-item="{iid}"]').drag_to(setlist.locator('.song-row').first)
 assert len(doc()['items'])==items;assert len(doc()['setlists'][0]['entries'])==count+1;assert doc()['setlists'][0]['entries'][0]['itemId']==iid;assert library.locator(f'.song-row[data-item="{iid}"]').count()==1
 page.keyboard.press('Control+z');assert len(doc()['setlists'][0]['entries'])==count
 rows=setlist.locator('.show-rows');box=rows.bounding_box();library.locator(f'.song-row[data-item="{iid}"]').drag_to(rows,target_position={'x':10,'y':box['height']-3});assert doc()['setlists'][0]['entries'][-1]['itemId']==iid;assert len(doc()['items'])==items
 page.keyboard.press('Control+z');assert len(doc()['setlists'][0]['entries'])==count
 page.screenshot(path='/tmp/mr-live-items-sidebar.png')
 # Close side browser to expose the track lane.
 library.get_by_role('button',name='Закрыть',exact=True).click()
 d=doc();c=next(c for c in d['channels'] if c['type']=='Player');cid=c['id'];iid=d['items'][0]['id'];pid=c['patches'][0]['id']
 first=page.locator('.live-item-container').first;row=first.locator(f'.item-track[data-channel="{cid}"]');card=row.locator('.track-patch').first
 for name in ['Rhythm','Solo']:
  card.get_by_role('button',name='+ Sub-patch',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_label('Название sub-patch',exact=True).fill(name);modal.get_by_role('button',name='Сохранить',exact=True).click()
 row.get_by_role('button',name='+ Патч',exact=True).click();modal=page.get_by_role('dialog');modal.get_by_label('Название патча',exact=True).fill('Lead');modal.get_by_role('button',name='Сохранить',exact=True).click()
 def track():return next(t for i in doc()['items'] if i['id']==iid for t in i['tracks'] if t['channelId']==cid)
 def drag(target,mode,delta,limit):
  handle=target.locator(':scope > .extent-'+mode) if mode!='move' else target.locator('.track-patch-title') if target.locator('.track-patch-title').count() else target
  handle.scroll_into_view_if_needed();px=target.locator('..').bounding_box()['width']/limit;box=handle.bounding_box();x=box['x']+box['width']/2;y=box['y']+box['height']/2;page.mouse.move(x,y);page.mouse.down();page.mouse.move(x+delta*px,y,steps=10);page.mouse.up()
 def near(a,b):assert math.isclose(a,b,abs_tol=.25),(a,b)
 def nonoverlap(values):
  values=sorted(values,key=lambda s:s['start']);assert all(a['start']+a['length']<=b['start']+1e-6 for a,b in zip(values,values[1:]))
 bars=doc()['items'][0]['bars'];old=track();ids=old['patchOrder'];other=next(i for i in ids if i!=pid);card=row.locator(f'[data-patch="{pid}"]')
 drag(card,'right',3,bars);now=track();near(now['spans'][pid]['length'],old['spans'][pid]['length']+3);near(now['spans'][other]['start'],old['spans'][other]['start']+3);nonoverlap(now['spans'].values())
 page.keyboard.press('Control+z');assert track()==old;page.keyboard.press('Control+Shift+z');assert track()==now
 othercard=row.locator(f'[data-patch="{other}"]');drag(othercard,'left',-2,bars);near(track()['spans'][other]['start'],now['spans'][other]['start']-2);near(track()['spans'][pid]['length'],now['spans'][pid]['length']-2);page.keyboard.press('Control+z');assert track()==now
 # Cancellation restores both displayed neighbours without changing storage.
 grip=card.locator(':scope > .extent-right');grip.scroll_into_view_if_needed();box=grip.bounding_box();page.mouse.move(box['x']+3,box['y']+8);page.mouse.down();page.mouse.move(box['x']+24,box['y']+8,steps=4);card.dispatch_event('pointercancel',{'pointerId':1});page.mouse.up();assert track()==now
 # Parent shrink scales nested strips, retaining adjacency.
 subs=track()['spans'][pid]['subs'];sid=list(subs)[0];nextsid=list(subs)[1];sub=card.locator(f'[data-subpatch="{sid}"]');length=track()['spans'][pid]['length']
 drag(sub,'right',2,length);newsubs=track()['spans'][pid]['subs'];near(newsubs[sid]['length'],subs[sid]['length']+2);near(newsubs[nextsid]['start'],subs[nextsid]['start']+2);nonoverlap(newsubs.values())
 before=track()['spans'];drag(sub,'move',4,length);assert track()['spans']==before
 # All three channel types have one global gain, unaffected by patch/sub/item selection.
 page.locator('[data-action="view.mixer"]').click();mixer=page.locator('.runtime-mixer');assert mixer.is_visible()
 assert all(r['width']<=122 for r in mixer.locator('.mixer-strip').evaluate_all('(nodes)=>nodes.map(n=>n.getBoundingClientRect().toJSON())'))
 for kind in ['Instrument','Player','Playback']:
  channel=next(c for c in doc()['channels'] if c['type']==kind);strip=mixer.locator(f'.mixer-strip[data-channel="{channel["id"]}"]');slider=strip.get_by_role('slider',name='Громкость '+channel['name'],exact=True);slider.fill('-8');slider.dispatch_event('input');slider.dispatch_event('change');assert next(c for c in doc()['channels'] if c['id']==channel['id'])['gain']==-8
 mixer.get_by_role('button',name='Закрыть',exact=True).click();card.locator('.track-patch-title').click();sub.click()
 assert all(c['gain']==-8 for c in doc()['channels'] if c['type'] in ['Instrument','Player','Playback'])
 page.locator('.live-item-container').nth(1).get_by_role('button',name='Открыть item',exact=True).click();assert all(c['gain']==-8 for c in doc()['channels'] if c['type'] in ['Instrument','Player','Playback'])
 saved=doc();page.reload();page.wait_for_selector('.block');assert doc()==saved;assert not errors,errors
 page.locator('#preview').check();page.locator('[data-action="view.mixer"]').click();page.screenshot(path='/tmp/mr-live-compact-mixer.png');b.close()
print('PASS: mixer palette/compact strips, sidebar copy-to-setlist, joined patch/sub edges, collision clamp, global gain, Undo/Redo/reload')
