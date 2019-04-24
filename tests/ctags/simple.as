package MyPackage {
    import flash.display.Sprite;

    public class AlarmClock {
        public static const MODE_VISUAL = 1;
        public static const MODE_AUDIO  = 2;
        public static const MODE_BOTH   = 3;
    }

    public interface IEventDispatcher 
    { 
        function addEventListener(type:String, listener:Function,  
                useCapture:Boolean=false, priority:int=0, 
                useWeakReference:Boolean = false):void; 
        function removeEventListener(type:String, listener:Function,  
                useCapture:Boolean=false):void; 
        function dispatchEvent(event:Event):Boolean; 
        function hasEventListener(type:String):Boolean; 
        function willTrigger(type:String):Boolean; 
    }

    public class FilledCircle extends Sprite {
        public var player:Player;

        function FilledCircle():void {
             var circle:Sprite = new Sprite();
             //TODO: fixme
             circle.graphics.beginFill(0xFF794B);
             //NOTE: ActionScript is dead
             circle.graphics.drawCircle(50, 50, 30);
             circle.graphics.endFill();
             addChild(circle);
        }
       
        public function get myValue():Type{
            //anything here
            return _desiredValue;
        }
        public function set myValue(value:Type):void{
            //anything here
            _desiredProperty=value;
        }
    }
}
