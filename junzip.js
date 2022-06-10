// fn to read string from wasm
var read_string_pointer = (function () {
    var charCache = new Array(128);  // Preallocate the cache for the common single byte chars
    var charFromCodePt = String.fromCodePoint || String.fromCharCode;
    var result = [];

    return function (ptr,array,maxLen) {
        var codePt, byte1;
        var startPos=ptr;
        var endPos=Math.min(array.length,startPos+maxLen);
        

        result.length = 0;

        for (var i = startPos; i < endPos;) {
            byte1 = array[i++];
            if(byte1==0)
            {
                // null termination, stop
                break;
            }

            if (byte1 <= 0x7F) {
                codePt = byte1;
            } else if (byte1 <= 0xDF) {
                codePt = ((byte1 & 0x1F) << 6) | (array[i++] & 0x3F);
            } else if (byte1 <= 0xEF) {
                codePt = ((byte1 & 0x0F) << 12) | ((array[i++] & 0x3F) << 6) | (array[i++] & 0x3F);
            } else if (String.fromCodePoint) {
                codePt = ((byte1 & 0x07) << 18) | ((array[i++] & 0x3F) << 12) | ((array[i++] & 0x3F) << 6) | (array[i++] & 0x3F);
            } else {
                codePt = 63;    // Cannot convert four byte code points, so use "?" instead
                i += 3;
            }
            result.push(charCache[codePt] || (charCache[codePt] = charFromCodePt(codePt)));
        }

        return result.join('');
    };
})();
    


async function init(callback)
{
    var imports = {
        env: {
            console_log: function(arg) { console.log("HERE:",arg,";",read_string_pointer(arg,zipMod.HEAPU8,1024)); },
            onUnzippedFile: callback,
            setTempRet0: function(arg){},
            getTempRet0: function(arg){},
        }
    };  
    var module=await WebAssembly.instantiateStreaming(fetch("junzip.wasm"),imports);
    var instance=module.instance;
    instance.HEAPU8=new Uint8Array(instance.exports.memory.buffer);
    return instance;
}



function fileCallback(buf,size,filenamePtr,nameLen)
{
    // transfer filename 
    //copy the file into a new uint8array
    // post that to owner
    fname=read_string_pointer(filenamePtr,zipMod.HEAPU8,nameLen);
    fileBytes=zipMod.HEAPU8.slice(buf,buf+size);
 /*   if(fname.endsWith(".txt"))
    {
        fileText=read_string_pointer(buf,zipMod.HEAPU8,size);
        console.log(buf,fname,fileBytes,size,fileText);
    }else
    {
        console.log(buf,fname,fileBytes,size);
    }*/
}

console.time("TEST");
var zipMod=await init(fileCallback);
let response=await fetch("http://localhost:8000/numpy-1.22.3-cp310-cp310-emscripten_2_0_27_wasm32.whl");
let ab=new Uint8Array(await response.arrayBuffer());
// make string
// make data buff
memptr=zipMod.exports.malloc(ab.byteLength);
zipMod.HEAPU8.set(ab,memptr);
let retVal=zipMod.exports.addUnzipData(memptr,ab.byteLength);
zipMod.exports.free(memptr);
console.timeEnd("TEST");
