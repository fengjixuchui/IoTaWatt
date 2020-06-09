#include "IotaWatt.h"
#include "IotaScript.h"

const char*      unitstr[] = {
                    "Watts",
                    "Volts", 
                    "Amps", 
                    "VA", 
                    "Hz", 
                    "Wh", 
                    "kWh", 
                    "PF",
                    "VAR",
                    "VARh",
                    ""
                    };

uint8_t     unitsPrecision[] = { 
                    /*Watts*/ 2,
                    /*Volts*/ 2, 
                    /*Amps*/  3, 
                    /*VA*/    2, 
                    /*Hz*/    2, 
                    /*Wh*/    4, 
                    /*kWh*/   7, 
                    /*PF*/    3,
                    /*VAR*/   2,
                    /*VARh*/  4,
                    /*None*/  0 
                    };                   

Script::Script(JsonObject& JsonScript)
      :_next(nullptr)
      ,_name(nullptr)
      ,_constants(nullptr)
      ,_tokens(nullptr)
      ,_units(Watts)
      
    {
      JsonVariant var = JsonScript["name"];
      if(var.success()){
        _name = charstar(var.as<char*>());
      }
    
      _units = Watts;
      var = JsonScript["units"];
      if(var.success()){
        for(int i=0; i<unitsNone; i++){
          if(strcmp_ci(var.as<char*>(),unitstr[i]) == 0){
            _units = (units)i;
            break;
          } 
        }
      }

      var = JsonScript["script"];
      if(var.success()){
        encodeScript(var.as<char*>());
      }
    }

Script::Script(const char* name, const char* unit, const char* script)
      :_next(nullptr)
      ,_name(nullptr)
      ,_constants(nullptr)
      ,_tokens(nullptr)
      ,_units(Watts)
       
    {
      _name = charstar(name);

       for(int i=0; i<unitsNone; i++){
          if(strcmp_ci(unit,unitstr[i]) == 0){
            _units = (units)i;
            break;
          } 
        }
      encodeScript(script);
    }

Script::~Script() {
      delete[] _name;
      delete[] _tokens;
      delete[] _constants;
    }

Script*       Script::next() {return _next;}

const char*   Script::name() {return _name;} 

const char*   Script::getUnits() {return unitstr[_units];};

int           Script::precision(){return unitsPrecision[_units];};

size_t        ScriptSet::count() {return _count;}

Script*       ScriptSet::first() {return _listHead;}  

void    Script::print() {
        uint8_t* token = _tokens;
        String string = "Script:";
        string += _name;
        string += ",units:";
        string += _units;
        string += ' ';
        while(*token){
          if(*token < getInputOp){
            string += String(opChars[*token]);
          }
          else if(*token & getInputOp){
            string += "@" + String(*token - getInputOp);
          }
          else if(*token & getConstOp){
            string += String(_constants[*token - getConstOp],4);
            while(string.endsWith("0")) string.remove(string.length()-1);
            if(string.endsWith(".")) string += '0';
          }
          else {
            string += "token(" + String(*token) + ")";
          }
          token++;
        }
        Serial.println(string);
}

bool    Script::encodeScript(const char* script){
        int tokenCount = 0;
        int constCount = 0;
        int consts = constCount;
        for(int i=0; i<strlen(script); i++){
          if(script[i] == '#')constCount++;
          if((!isDigit(script[i])) && (script[i] != '.')) tokenCount++;
        }
        _tokens = new uint8_t[tokenCount + 1];
        _constants = new float[constCount];
        int j = 0;
        int i = 0;     
        while(script[j]){
          if(script[j] == '@'){
            char* endptr;
            int n = strtol(&script[j+1], &endptr, 10);
            j = endptr - script;
            _tokens[i++] = getInputOp + n;
          } 
          else if (script[j] == '#'){
            _tokens[i++] = getConstOp + --constCount;
            char* endptr;
            _constants[constCount] = strtof(&script[j+1], &endptr);
            j = endptr - script;
          }
          else {
            _tokens[i++] = strchr(opChars, script[j++]) - opChars;
          }
        }
        _tokens[i] = 0;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, const char* overideUnits){
        for(int i=0; i<unitsNone; i++){
          if(strcmp_ci(overideUnits,unitstr[i]) == 0){
            return run(oldRec, newRec, elapsedHours, (units) i);
          } 
        }
        return 0;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, units overideUnits){
        units defaultUnits = _units;
        _units = overideUnits;
        double result = run(oldRec, newRec, elapsedHours);
        _units = defaultUnits;
        return result;
}

double  Script::run(IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours){
        uint8_t* tokens = _tokens;
        double result, var, watts;
        switch(_units) {

          case Watts:
          case Volts:
            result = runRecursive(&tokens, oldRec, newRec, elapsedHours, '1'); 
            break;

          case Wh:
            result = runRecursive(&tokens, oldRec, newRec, 1.0, '1'); 
            break;

          case kWh:
            result = runRecursive(&tokens, oldRec, newRec, 1000.0, '1'); 
            break;
            
          case Amps:
            result = runRecursive(&tokens, oldRec, newRec, elapsedHours, 'A'); 
            break;

          case VA:
            var = runRecursive(&tokens, oldRec, newRec, elapsedHours, 'R');
            watts = runRecursive(&tokens, oldRec, newRec, elapsedHours, '1');
            result = sqrt(watts*watts + var*var); 
            break;

          case Hz:
            result = runRecursive(&tokens, oldRec, newRec, elapsedHours, 'H'); 
            break;

          case PF:
            watts = runRecursive(&tokens, oldRec, newRec, elapsedHours, '1');
            var = runRecursive(&tokens, oldRec, newRec, elapsedHours, 'R');
            result = watts / sqrt(watts*watts + var*var); 
            break;

          case VAR:
            result = runRecursive(&tokens, oldRec, newRec, elapsedHours, 'R');
            break;

          case VARh:
            result = runRecursive(&tokens, oldRec, newRec, 1.0, 'R');
            break;
        }
        
        if(result != result) return 0.0;
        return result;
                
}

double  Script::runRecursive(uint8_t** tokens, IotaLogRecord* oldRec, IotaLogRecord* newRec, double elapsedHours, char type){
        double result = 0.0;
        double operand = 0.0;
        uint8_t pendingOp = opAdd;
        uint8_t* token = *tokens;
        do {
          //Serial.printf("token %d, result %f, operand %f, pendingop %d\r\n", *token, result, operand, pendingOp);
          if(*token >= opAdd && *token <= opMax){
            result = evaluate(result, pendingOp, operand);
            pendingOp = *token;
            operand = 0;
            if(*token == opDiv || *token == opMult) operand = 1;
          }
          else if(*token == opAbs){
            if(operand < 0) operand = 0 - operand;
          }       
          else if(*token == opPush){
            token++;
            operand = runRecursive(&token, oldRec, newRec, elapsedHours, type);
          }
          else if(*token == opPop){ 
            *tokens = token;
            return evaluate(result, pendingOp, operand);
          }
          else if(*token == opEq){
            return evaluate(result, pendingOp, operand);
          }
          if(*token & getConstOp){
            operand = _constants[*token % 32];
          }

              // Fetch input operand.
              // accum1 is Wh, Vh
              // accum2 is VAh, Hzh
              // Type 1 retieves accum1
              // Type 2 retrieves accum2
              // Type R computes var as sqrt(VA^2 - W^2)
              // Type A computes Amps as VA / V
              // Type H retrieves Hz for associated voltage channel

          if(*token & getInputOp){
            if(type == '1'){
              operand = (newRec->accum1[*token % 32] - (oldRec ? oldRec->accum1[*token % 32] : 0.0)) / elapsedHours;
            }
            else if(type == '2'){
              operand = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0)) / elapsedHours;
            }
            else if(type == 'R'){
              double _VA = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0)) / elapsedHours;
              double W = (newRec->accum1[*token % 32] - (oldRec ? oldRec->accum1[*token % 32] : 0.0)) / elapsedHours;
              operand = sqrt(_VA*_VA - W*W);
            }
            else if(type == 'A'){
              if(*token % 32 >= maxInputs) return 0.0;
              double _VA = (newRec->accum2[*token % 32] - (oldRec ? oldRec->accum2[*token % 32] : 0.0)) / elapsedHours;
              int vchannel = inputChannel[*token % 32]->_vchannel;
              operand = ((newRec->accum1[vchannel] - (oldRec ? oldRec->accum1[vchannel] : 0.0)) / elapsedHours);
              if(operand != 0.0){
                operand = _VA / operand;
              }
              if(inputChannel[*token % 32]->_double){
                operand /= 2.0;
              }
            }
            else if(type == 'H'){
              if(*token % 32 >= maxInputs) return 0.0;
              int vchannel = inputChannel[*token % 32]->_vchannel;
              operand = (newRec->accum2[vchannel] - (oldRec ? oldRec->accum2[vchannel] : 0.0)) / elapsedHours;
            }
            else operand = 0.0;
            if(operand != operand) operand = 0;
          }

        } while(token++);
}

double    Script::evaluate(double result, uint8_t token, double operand){
        switch (token) {
          case opAdd:  return result + operand;
          case opSub:  return result - operand;
          case opMult: return result * operand;
          case opDiv:  return operand == 0 ? 0 : result / operand;
          case opMin:  return result < operand ? result : operand;
          case opMax:  return result > operand ? result : operand;
          default:     return 0;        
        }
}

          // Sort the Scripts in the set
          // Uses callback comparison
          // Simple bubble sort

void  ScriptSet::sort(std::function<int(Script*, Script*)> scriptCompare){
  int count = _count;
  while(--count){
    Script* link = nullptr;
    Script* a = _listHead;
    Script* b = a->_next;
    for(int i=0; i<count; i++){
      int comp = scriptCompare(a, b);
      if( comp > 0){
        a->_next = b->_next;
        b->_next = a;
        if(link){
          link->_next = b;
        } else {
          _listHead = b;
        }
        link = b;
      } else {
        link = a;
      }
      a = link->_next;
      b = a->_next;
    }
  }
}