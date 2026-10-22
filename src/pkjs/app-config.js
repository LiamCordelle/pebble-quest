'use strict';
function normalizeName(name) {
  name=typeof name==='string'?name:'LIAM';
  name=name.replace(/[^A-Za-z0-9 ]/g,'').toUpperCase().slice(0,8).trim();
  return name||'LIAM';
}
module.exports={heading:'Quest',storageKey:'quest-kit-settings-v1',largeLabels:false,
  faceSettings:[{type:'input',messageKey:'HERO_NAME',label:'Hero name',defaultValue:'LIAM',
    attributes:{maxlength:8},description:'Up to 8 letters, numbers or spaces.'}],
  normalizeFaceSetting:function(key,value){return key==='HERO_NAME'?normalizeName(value):value;}
};
