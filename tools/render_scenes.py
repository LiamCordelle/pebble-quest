#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,json
ROOT=Path(__file__).resolve().parents[1]
LAB=Path(os.environ.get('WATCHFACE_LAB',ROOT.parent/'watchface-lab'))
scenes=[('demo',0,{}),('stress',1,{}),('empty',2,{})]
for name,env in [
 ('12h',{'12H':1}),('12h-midnight',{'12H':1,'HOUR':0,'MINUTE':1}),
 ('12h-evening',{'12H':1,'HOUR':23,'MINUTE':59}),('midnight',{'HOUR':0,'MINUTE':1}),
 ('after-sunset',{'HOUR':20,'MINUTE':48}),('phone-lost',{'PHONE':0}),
 ('low-battery',{'BATTERY':5}),('charging',{'CHARGING':1}),
 ('no-health',{'NO_HEALTH':1}),('partial-range',{'HR_PARTIAL':1}),
 ('no-range',{'NO_RANGE':1}),('fahrenheit',{'FAHRENHEIT':1}),
 ('hero8',{'HERO_NAME':'WIZARD99'}),('tomorrow-rain',{'HOUR':22,'CROSS_MIDNIGHT':1}),
 ('tomorrow-rain-12h',{'HOUR':22,'12H':1,'CROSS_MIDNIGHT':1}),
 ('no-sun',{'NO_SUN':1}),('goal',{'STEPS':18765,'DRY':1}),
 ('high-uv',{'UV':110,'DRY':1}),('rest',{'HOUR':8,'DRY':1}),
 ('no-rain',{'DRY':1}),('wide-stats',{'ACTIVE':1440,'DISTANCE':123456,'SLEEP':1439,'HR_MIN':110,'HR_MAX':255,'HR_PARTIAL':1})]:
 scenes.append((name,0,env))
for kind,code in [('sun',0),('partly',2),('cloud',3),('rain',63),('snow',73),('storm',95),('fog',45)]:
 for hour,period in [(10,'day'),(23,'night')]:scenes.append((kind+'-'+period,0,{'WEATHER_CODE':code,'HOUR':hour}))
for name,scenario,values in scenes:
 env=os.environ.copy();env.update({'QUEST_'+k:str(v) for k,v in values.items()})
 for detail in (False,True):
  out=ROOT/'shots'/(name+('-detail' if detail else '')+'.png')
  command=['python3',str(LAB/'tools/render.py'),str(ROOT),str(out),'--scenario',str(scenario)]
  if detail:command.append('--detail')
  result=subprocess.run(command,env=env,capture_output=True,text=True)
  if result.returncode:raise RuntimeError(name+'\n'+result.stdout+result.stderr)
print(f'Passed {len(scenes)*2} renders, with measured text bounds on every important line.')
for name in ('demo','demo-detail'):
 subprocess.run(['python3',str(LAB/'tools/glance.py'),str(ROOT/'shots'/(name+'.png'))],check=True)
(ROOT/'shots/coverage.json').write_text(json.dumps([{'name':n,'scenario':s,'overrides':v} for n,s,v in scenes],indent=2)+'\n')
