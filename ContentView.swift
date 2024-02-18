//
//  ContentView.swift
//  SeniorDesignApp
//
//  Created by Davd Clinkscales on 1/26/24.
//

import SwiftUI
import SwiftData

import SwiftUI

struct ContentView: View {
    var body: some View {
        NavigationView{
        VStack {
            Text("Respiratory")
                .font(.largeTitle).bold()
            VStack {
                Text("Alert").font(.largeTitle).bold()
            }
            
                VStack {
                    NavigationLink(destination:LockScreen()){
                        Image(systemName: "lock.circle.fill").resizable()
                            .aspectRatio(contentMode: .fit)
                            .frame(width: 175, height: 175)
                            .fixedSize()
                    }
                    VStack {
                        Text("Tap Lock to Unlock").padding(.top,15)
                    }
            }.padding(.top,150)
           
            }
            
        }
        
    
    }
}

struct ContentView_Previews:
    PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
