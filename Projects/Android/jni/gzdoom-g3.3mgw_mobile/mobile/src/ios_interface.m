//
//  ios_interface.m
//  D-Touch
//
//  Created by Emile Belanger on 17/01/2016.
//  Copyright © 2016 Beloko Games. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "ios_interface.h"

NSString * pngDir;

const char * getPngDirectory()
{
    pngDir = [[NSBundle mainBundle] resourcePath];
    pngDir  = [pngDir stringByAppendingString:@"/png/"];
    
    return [pngDir cStringUsingEncoding:[NSString defaultCStringEncoding]];
}

