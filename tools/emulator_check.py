#!/usr/bin/env python3
"""Final Emery check. Run with pebble-tool's Python; always restores its profile.
Weather is synthetic test data injected through the real AppMessage protocol.
"""
from pathlib import Path
import os,json,re,subprocess,sys,tempfile,shutil,time,threading
from uuid import UUID
from libpebble2.communication import PebbleConnection
from libpebble2.protocol.apps import AppRunState,AppRunStateRequest,AppRunStateStart,AppRunStateStop
from libpebble2.communication.transports.websocket import MessageTargetPhone
from libpebble2.communication.transports.websocket.protocol import AppConfigSetup,AppConfigResponse,WebSocketPhonesimAppConfig,WebSocketPhonesimConfigResponse
from libpebble2.services.appmessage import AppMessageService,Uint32,ByteArray,CString
from pebble_tool.sdk.emulator import ManagedEmulatorTransport
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'shots/emulator';OUT.mkdir(parents=True,exist_ok=True)
os.chdir(ROOT)
if '--clean-profile' in sys.argv:
 from pebble_tool.sdk import get_sdk_persist_dir
 active=subprocess.run(['pgrep','-f','qemu-system|qemu-pebble'],capture_output=True)
 if active.returncode==0:raise RuntimeError('Another emulator is running; stop it before this check.')
 profile=Path(get_sdk_persist_dir('emery'));backup=Path(tempfile.mkdtemp(prefix='quest-qemu-'))/'profile';existed=profile.exists()
 if existed:shutil.move(str(profile),str(backup))
 try:result=subprocess.run([sys.executable,str(Path(__file__).resolve())]+(['--quick'] if '--quick' in sys.argv else []),cwd=ROOT)
 finally:
  subprocess.run(['pebble','kill'],capture_output=True)
  if profile.exists():shutil.rmtree(profile)
  if existed:shutil.move(str(backup),str(profile))
  backup.parent.rmdir();print('Original Emery profile restored; QEMU stopped.',flush=True)
 sys.exit(result.returncode)

def run(*args):
 result=subprocess.run(['pebble',*args,'--emulator','emery'],capture_output=True,text=True,timeout=60,check=True)
 with (OUT/'commands.txt').open('a') as f:f.write(' '.join(args)+'\n'+result.stdout+result.stderr+'\n')
 if args[0] in ('emu-battery','emu-tap','emu-button','emu-bt-connection'):time.sleep(.8)
 return result

def shot(name):run('screenshot','--no-open',str(OUT/(name+'.png')))
pkg=json.loads((ROOT/'package.json').read_text());uuid=UUID(pkg['pebble']['uuid'])
keys=json.loads((ROOT/'build/js/message_keys.json').read_text())
connection=None;service=None;sent=threading.Event()
with (OUT/'emulator.log').open('w') as log:
 env=os.environ.copy();env['PYTHONUNBUFFERED']='1'
 logs=subprocess.Popen(['pebble','logs','--emulator','emery'],stdout=log,stderr=subprocess.STDOUT,env=env)
 try:
  print('Booting clean Emery profile and capturing startup logs.',flush=True);time.sleep(20)
  run('emu-steps','6842');run('emu-battery','--percent','76');run('install')
  connection=PebbleConnection(ManagedEmulatorTransport('emery'));connection.connect();connection.run_async()
  service=AppMessageService(connection);service._current_txid=180
  connection.send_packet(AppRunState(data=AppRunStateStart(uuid=uuid)));time.sleep(2)
  current=connection.send_and_read(AppRunState(data=AppRunStateRequest()),AppRunState)
  assert current.data.uuid==uuid,'Wrong active app UUID'
  print('Verified Quest active UUID.',flush=True)
  def settings(name='LIAM',enabled=0,mode=2,unit=0):
   v={'READY':1,'TIME_MODE':mode,'TEMP_UNIT':unit,'STEP_GOAL':10000,'WORLD':0,
      'WEATHER_ENABLED':enabled,'DEMO_FALLBACK':0,'LARGE_LABELS':0}
   payload={keys[k]:Uint32(n) for k,n in v.items()};payload[keys['HERO_NAME']]=CString(name)
   service.send_message(uuid,payload);time.sleep(1)
  settings()
  for _ in range(3):run('emu-heart-rate','64');time.sleep(1)
  print('Checking live health and detail lifecycle.',flush=True)
  if '--quick' in sys.argv:time.sleep(5)
  else:
   for _ in range(2):time.sleep(35)
  run('emu-tap');run('emu-tap');shot('live-face')
  run('emu-tap');shot('live-detail');time.sleep(8);shot('timeout')
  from PIL import Image,ImageChops
  # Clock and health can both change while detail is open. The distinctive
  # four main-window frame lines establish the returned view independently.
  for y in (2,68,106,186):
   a=Image.open(OUT/'live-face.png').convert('RGB').crop((10,y,190,y+2))
   b=Image.open(OUT/'timeout.png').convert('RGB').crop((10,y,190,y+2))
   assert not ImageChops.difference(a,b).getbbox(),'Detail failed to return to face'
  # Fresh forecast fixture uses current UTC midnight; no public API or location claims.
  script="""var fs=require('fs'),wx=require('./shared-kit/src/pkjs/kit/weather');
var now=Math.floor(Date.now()/1000),base=now-now%86400,h={time:[],temperature_2m:[],precipitation_probability:[],weather_code:[],uv_index:[]};
for(var i=0;i<48;i++){h.time.push(base+i*3600);h.temperature_2m.push(14);h.precipitation_probability.push(i===Math.floor((now-base)/3600)+2?60:20);h.weather_code.push(2);h.uv_index.push(3);}
var d={utc_offset_seconds:0,current:{time:now,temperature_2m:14,relative_humidity_2m:72,weather_code:2,wind_speed_10m:12,wind_direction_10m:225},hourly:h,
daily:{time:[base,base+86400],sunrise:[base+7*3600,base+86400+7*3600],sunset:[base+18*3600,base+86400+18*3600],temperature_2m_max:[17,17],temperature_2m_min:[9,9],uv_index_max:[4,4]}};
fs.writeFileSync('build/emulator-weather.bin',Buffer.from(wx.encode(d,now)));"""
  subprocess.run(['node','-e',script],cwd=ROOT,check=True)
  weather=(ROOT/'build/emulator-weather.bin').read_bytes()
  def incoming(tid,app,data):
   if app==uuid and keys['REQUEST_WEATHER'] in data and not sent.is_set():
    sent.set();service.send_message(uuid,{keys['WEATHER_ID']:Uint32(data[keys['REQUEST_WEATHER']]),keys['WEATHER']:ByteArray(weather)})
  service.register_handler('appmessage',incoming)
  # Exercise the actual Clay response path while phone weather is disabled.
  phone_values={'TIME_MODE':1,'TEMP_UNIT':1,'STEP_GOAL':10000,'WORLD':0,
    'WEATHER_ENABLED':False,'DEMO_FALLBACK':False,'LARGE_LABELS':False,'HERO_NAME':'wizard99'}
  connection.transport.send_packet(WebSocketPhonesimAppConfig(config=AppConfigSetup()),target=MessageTargetPhone())
  connection.read_transport_message(MessageTargetPhone,WebSocketPhonesimConfigResponse,timeout=10)
  connection.transport.send_packet(WebSocketPhonesimAppConfig(config=AppConfigResponse(data=json.dumps(phone_values))),target=MessageTargetPhone())
  time.sleep(2);shot('clay-settings')
  settings('WIZARD99',1,1,1)
  assert sent.wait(10),'No correlated weather request'
  time.sleep(2);shot('settings-weather');run('emu-tap');shot('weather-detail');run('emu-tap')
  print('Hero name, 12-hour/Fahrenheit and correlated weather applied.',flush=True)
  # Save matching phone settings too: a phone ready snapshot must not disable
  # weather and deliberately clear the watch cache after a restart.
  phone_values['WEATHER_ENABLED']=True
  connection.transport.send_packet(WebSocketPhonesimAppConfig(config=AppConfigSetup()),target=MessageTargetPhone())
  connection.read_transport_message(MessageTargetPhone,WebSocketPhonesimConfigResponse,timeout=10)
  connection.transport.send_packet(WebSocketPhonesimAppConfig(config=AppConfigResponse(data=json.dumps(phone_values))),target=MessageTargetPhone())
  time.sleep(1)
  connection.send_packet(AppRunState(data=AppRunStateStop(uuid=uuid)));time.sleep(1)
  connection.send_packet(AppRunState(data=AppRunStateStart(uuid=uuid)));time.sleep(2)
  active=connection.send_and_read(AppRunState(data=AppRunStateRequest()),AppRunState)
  assert active.data.uuid==uuid
  shot('cold-restart')
  # Only the first weather request is answered by the harness. This crop must
  # therefore come from the persisted snapshot, not another synthetic reply.
  before=Image.open(OUT/'settings-weather.png').convert('RGB').crop((0,68,200,104))
  after=Image.open(OUT/'cold-restart.png').convert('RGB').crop((0,68,200,104))
  assert not ImageChops.difference(before,after).getbbox(),'Weather did not survive cold restart'
  # Hero pixels plus 12-hour mode also survive without rewriting on the watch.
  before=Image.open(OUT/'settings-weather.png').convert('RGB').crop((10,114,125,128))
  after=Image.open(OUT/'cold-restart.png').convert('RGB').crop((10,114,125,128))
  assert not ImageChops.difference(before,after).getbbox(),'Hero name did not survive cold restart'
  run('emu-battery','--percent','5');shot('critical')
  run('emu-battery','--percent','62','--charging');shot('charging')
  run('emu-bt-connection','--connected','no');time.sleep(27);shot('disconnected')
  run('emu-bt-connection','--connected','yes');run('emu-battery','--percent','76')
  settings('LIAM',1,2,0);shot('final-face');run('emu-tap');shot('final-detail');run('emu-tap')
  connection.send_packet(AppRunState(data=AppRunStateStop(uuid=uuid)));time.sleep(1)
  print('Final face/detail, timeout, status and restart evidence captured.',flush=True)
 finally:
  if service:service.shutdown()
  if connection:connection.transport.ws.close()
  logs.terminate()
  try:logs.wait(timeout=5)
  except subprocess.TimeoutExpired:logs.kill();logs.wait()
  subprocess.run(['pebble','kill'],capture_output=True)
text=(OUT/'emulator.log').read_text()
assert not re.search(r'APP_FAULT|stack overflow|app crash',text,re.I),text
assert 'Hero name applied: WIZARD99' in text,'No hero settings event'
assert 'HR minute history ready:' in text,'No recorded-minute HR history scan'
frames=[int(v) for v in re.findall(r'Frame \w+ (\d+)ms',text)]
assert frames
valid_frames=[v for v in frames if v<10000]
invalid_frames=[v for v in frames if v>=10000]
assert valid_frames
assert 'Weather snapshot accepted' in text
assert len(re.findall(r'Watchface kit \S+ started',text))>=2,'No cold app initialization'
assert 'Still allocated <0B>' in text,'No clean app teardown evidence'
(OUT/'result.json').write_text(json.dumps({'uuid':str(uuid),'frames_ms':valid_frames,'invalid_wall_clock_samples':invalid_frames,'cold_restart_verified':True,'synthetic_weather':True,'no_faults':True},indent=2)+'\n')
print(f'Quest final check passed; frame wall times {min(valid_frames)}–{max(valid_frames)}ms; {len(invalid_frames)} clock discontinuities.',flush=True)
