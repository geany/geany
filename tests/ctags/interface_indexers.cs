// cs_interface_indexers.cs
using System;
// Indexer on an interface:
public interface IMyInterface 
{
   // indexer declaration:
   int this[int index] 
   {
      get; 
      set; 
   }
}

// Implementing the interface:
class IndexerClass : IMyInterface
{
   private int [] myArray = new int[100]; 
   public int this [int index]   // indexer declaration
   {
      get 
      {
         // Check the index limits
         if (index < 0 || index >= 100)
            return 0;
         else
            return myArray[index];
      }
      set 
      {
         if (!(index < 0 || index >= 100))
            myArray[index] = value;
      }
   }
}

public class MainClass 
{
   public static void Main() 
   {
      IndexerClass b = new IndexerClass();
      // call the indexer to initialize the elements #3 and #5:
      b[2] = 4;
      b[5] = 32;
      for (int i=0; i<=10; i++) 
      {
         Console.WriteLine("Element #{0} = {1}", i, b[i]);
      }
   }
}
