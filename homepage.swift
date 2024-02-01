//
//  homepage.swift
//  SeniorDesignApp
//
//  Created by Davd Clinkscales on 1/29/24.
//

import SwiftUI

struct homepage: View {
    @Environment(\.presentationMode) var presenttionMode: Binding<PresentationMode>
    @State var Comm = false
    
    var body: some View {
        NavigationView{
            HStack{
                if Comm {
                    NavigationLink(destination:BLE()){
                        Circle().fill(Color.green).frame(width: 25, height: 25)
                        Text(" ~ Connected")
                    }
                    
                }
                else{
                    NavigationLink(destination:BLE()){
                        Circle()
                            .fill(Color.red)
                            .frame(width: 25, height: 25)
                        Text(" ~ Disconnected")
                    }
                    
                    
                }
                Spacer()
                
                VStack {
                    Image(systemName: "person.crop.circle").resizable().frame(width: 25, height: 25).padding()
                        
                }
                 
               
            }
            .navigationBarHidden(true)
            .navigationBarBackButtonHidden(true)
        }
       
        //        HStack{
        //
        //        }
        //        HStack{
        //
        //        }
        //    }
    }
    
    struct homepage_Previews: PreviewProvider {
        static var previews: some View {
            LockScreen()
        }
    }
}
